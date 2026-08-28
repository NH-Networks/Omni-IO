#include <esphome_server.h>

#if defined(ESPHOME_API)

#include <WiFi.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <fcntl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <iohcRemote1W.h>
#include <interact.h>
#include <log_buffer.h>
#include <nvs_helpers.h>
#include <utils.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

#ifdef FIRMWARE_VERSION
#define ESPHOME_FIRMWARE_VERSION FIRMWARE_VERSION
#else
#define ESPHOME_FIRMWARE_VERSION "3.3.0"
#endif

// FNV-1a 32-bit hash for stable entity keys across reboots
uint32_t espHomeFnv1a(const std::string &str) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : str) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

namespace {

// ==================== PROTOBUF CODEC ====================

class ProtoWriter {
public:
    std::vector<uint8_t> buffer;

    ProtoWriter() { buffer.reserve(128); }

    void writeVarint(uint64_t val) {
        while (val >= 0x80) {
            buffer.push_back(static_cast<uint8_t>((val & 0x7F) | 0x80));
            val >>= 7;
        }
        buffer.push_back(static_cast<uint8_t>(val & 0x7F));
    }

    void writeTag(uint32_t field, uint8_t wireType) {
        writeVarint((field << 3) | wireType);
    }

    void writeVarintField(uint32_t field, uint64_t val) {
        writeTag(field, 0);
        writeVarint(val);
    }

    void writeBoolField(uint32_t field, bool val) {
        writeTag(field, 0);
        writeVarint(val ? 1 : 0);
    }

    void writeFixed32Field(uint32_t field, uint32_t val) {
        writeTag(field, 5);
        buffer.push_back(static_cast<uint8_t>(val & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        buffer.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    }

    void writeFloatField(uint32_t field, float val) {
        union { float f; uint32_t u; } conv;
        conv.f = val;
        writeFixed32Field(field, conv.u);
    }

    void writeStringField(uint32_t field, const std::string &str) {
        if (str.empty()) return;
        writeTag(field, 2);
        writeVarint(str.size());
        buffer.insert(buffer.end(), str.begin(), str.end());
    }
};

class ProtoReader {
public:
    const uint8_t *data;
    size_t size;
    size_t offset;

    ProtoReader(const uint8_t *d, size_t s) : data(d), size(s), offset(0) {}

    bool hasNext() const { return offset < size; }

    bool readVarint(uint64_t &val) {
        val = 0;
        uint32_t shift = 0;
        while (offset < size) {
            uint8_t b = data[offset++];
            val |= static_cast<uint64_t>(b & 0x7F) << shift;
            if (!(b & 0x80)) return true;
            shift += 7;
            if (shift >= 64) return false;
        }
        return false;
    }

    bool readTag(uint32_t &field, uint8_t &wireType) {
        uint64_t raw = 0;
        if (!readVarint(raw)) return false;
        field = static_cast<uint32_t>(raw >> 3);
        wireType = static_cast<uint8_t>(raw & 0x07);
        return true;
    }

    bool skipField(uint8_t wireType) {
        switch (wireType) {
            case 0: { // varint
                uint64_t dummy;
                return readVarint(dummy);
            }
            case 1: { // 64-bit
                if (offset + 8 > size) return false;
                offset += 8;
                return true;
            }
            case 2: { // length-delimited
                uint64_t len = 0;
                if (!readVarint(len)) return false;
                if (offset + len > size) return false;
                offset += len;
                return true;
            }
            case 5: { // 32-bit
                if (offset + 4 > size) return false;
                offset += 4;
                return true;
            }
            default:
                return false;
        }
    }

    bool readFixed32(uint32_t &val) {
        if (offset + 4 > size) return false;
        val = static_cast<uint32_t>(data[offset]) |
              (static_cast<uint32_t>(data[offset + 1]) << 8) |
              (static_cast<uint32_t>(data[offset + 2]) << 16) |
              (static_cast<uint32_t>(data[offset + 3]) << 24);
        offset += 4;
        return true;
    }

    bool readFloat(float &val) {
        uint32_t u = 0;
        if (!readFixed32(u)) return false;
        union { uint32_t u; float f; } conv;
        conv.u = u;
        val = conv.f;
        return true;
    }

    bool readString(std::string &str) {
        uint64_t len = 0;
        if (!readVarint(len)) return false;
        if (offset + len > size) return false;
        str.assign(reinterpret_cast<const char*>(data + offset), len);
        offset += len;
        return true;
    }
};

// ==================== SESSION & STATE ====================

struct ClientSession {
    int socketFd = -1;
    bool authenticated = false;
    bool subscribedStates = false;
    uint32_t lastActivityMs = 0;
};

constexpr size_t MAX_CLIENTS = 4;
ClientSession s_clients[MAX_CLIENTS];
std::recursive_mutex s_clientsMutex;

int s_serverSocket = -1;
TaskHandle_t s_serverTaskHandle = nullptr;
bool s_serverRunning = false;

// Helpers to send plaintext framed messages
bool sendFrame(int sock, uint16_t msgType, const uint8_t *payload, size_t payloadLen) {
    ProtoWriter header;
    header.buffer.push_back(0x00); // Plaintext framing preamble
    header.writeVarint(payloadLen);
    header.writeVarint(msgType);

    ssize_t sentHdr = send(sock, header.buffer.data(), header.buffer.size(), 0);
    if (sentHdr < 0) return false;

    if (payloadLen > 0 && payload != nullptr) {
        ssize_t sentPayload = send(sock, payload, payloadLen, 0);
        if (sentPayload < 0) return false;
    }
    return true;
}

bool sendFrame(int sock, uint16_t msgType, const ProtoWriter &writer) {
    return sendFrame(sock, msgType, writer.buffer.data(), writer.buffer.size());
}

void broadcastFrameToSubscribers(uint16_t msgType, const ProtoWriter &writer) {
    std::lock_guard<std::recursive_mutex> lock(s_clientsMutex);
    for (size_t i = 0; i < MAX_CLIENTS; i++) {
        if (s_clients[i].socketFd >= 0 && s_clients[i].subscribedStates) {
            if (!sendFrame(s_clients[i].socketFd, msgType, writer)) {
                close(s_clients[i].socketFd);
                s_clients[i].socketFd = -1;
                s_clients[i].authenticated = false;
                s_clients[i].subscribedStates = false;
            }
        }
    }
}

// ==================== PROTOCOL HANDLERS ====================

void handleHelloRequest(int sock, ProtoReader &reader) {
    // HelloRequest has client_info (1), api_version_major (2), api_version_minor (3)
    // Respond with HelloResponse (2)
    ProtoWriter writer;
    writer.writeVarintField(1, 1);  // api_version_major = 1
    writer.writeVarintField(2, 10); // api_version_minor = 10
    writer.writeStringField(3, std::string("Omni-IO ") + ESPHOME_FIRMWARE_VERSION);
    writer.writeStringField(4, esphome_name.empty() ? "omni-io" : esphome_name);

    sendFrame(sock, EspHomeMsg::HELLO_RESPONSE, writer);
}

void handleConnectRequest(int sock, ProtoReader &reader, ClientSession &session) {
    std::string password;
    while (reader.hasNext()) {
        uint32_t field;
        uint8_t wireType;
        if (!reader.readTag(field, wireType)) break;
        if (field == 1 && wireType == 2) {
            reader.readString(password);
        } else {
            reader.skipField(wireType);
        }
    }

    bool invalidPassword = false;
    if (!esphome_password.empty() && password != esphome_password) {
        invalidPassword = true;
    }

    session.authenticated = !invalidPassword;

    ProtoWriter writer;
    writer.writeBoolField(1, invalidPassword); // invalid_password (1)
    sendFrame(sock, EspHomeMsg::CONNECT_RESPONSE, writer);
}

void handleDeviceInfoRequest(int sock) {
    ProtoWriter writer;
    writer.writeBoolField(1, !esphome_password.empty());                         // uses_password
    writer.writeStringField(2, esphome_name.empty() ? "omni-io" : esphome_name); // name
    writer.writeStringField(3, WiFi.macAddress().c_str());                        // mac_address
    writer.writeStringField(4, "2024.9.0");                                      // esphome_version
    writer.writeStringField(5, __DATE__ " " __TIME__);                           // compilation_time
    writer.writeStringField(6, "Omni-IO Gateway");                               // model
    writer.writeVarintField(10, 80);                                             // webserver_port = 80
    writer.writeStringField(12, "NH-Networks");                                  // manufacturer
    writer.writeStringField(13, "Omni-IO Gateway");                              // friendly_name

    sendFrame(sock, EspHomeMsg::DEVICE_INFO_RESPONSE, writer);
}

void sendCoverEntityList(int sock, const std::string &hexId, const std::string &name) {
    // 1. Cover Entity
    {
        std::string objId = "cover_" + hexId;
        uint32_t key = espHomeFnv1a(objId);

        ProtoWriter w;
        w.writeStringField(1, objId);         // object_id
        w.writeFixed32Field(2, key);          // key
        w.writeStringField(3, name);          // name
        w.writeStringField(4, objId);         // unique_id
        w.writeBoolField(5, false);           // assumed_state = false
        w.writeBoolField(6, true);            // supports_position = true
        w.writeBoolField(7, false);           // supports_tilt = false
        w.writeStringField(8, "blind");       // device_class = "blind"

        sendFrame(sock, EspHomeMsg::LIST_ENTITIES_COVER_RESPONSE, w);
    }

    // 2. Travel Time Number Entity
    {
        std::string objId = "cover_" + hexId + "_travel_time";
        uint32_t key = espHomeFnv1a(objId);

        ProtoWriter w;
        w.writeStringField(1, objId);                // object_id
        w.writeFixed32Field(2, key);                 // key
        w.writeStringField(3, name + " Travel Time");// name
        w.writeStringField(4, objId);                // unique_id
        w.writeStringField(5, "mdi:timer-outline");  // icon
        w.writeFloatField(6, 0.0f);                  // min_value
        w.writeFloatField(7, 120.0f);                // max_value
        w.writeFloatField(8, 1.0f);                  // step
        w.writeVarintField(10, 1);                   // entity_category = 1 (config)
        w.writeStringField(11, "s");                 // unit_of_measurement
        w.writeVarintField(12, 3);                   // mode = 3 (slider)

        sendFrame(sock, EspHomeMsg::LIST_ENTITIES_NUMBER_RESPONSE, w);
    }

    // 3. Pair Button
    {
        std::string objId = "cover_" + hexId + "_pair";
        uint32_t key = espHomeFnv1a(objId);

        ProtoWriter w;
        w.writeStringField(1, objId);                // object_id
        w.writeFixed32Field(2, key);                 // key
        w.writeStringField(3, name + " Pair");       // name
        w.writeStringField(4, objId);                // unique_id
        w.writeStringField(5, "mdi:link");           // icon
        w.writeVarintField(6, 1);                    // entity_category = 1 (config)

        sendFrame(sock, EspHomeMsg::LIST_ENTITIES_BUTTON_RESPONSE, w);
    }

    // 4. Add Button
    {
        std::string objId = "cover_" + hexId + "_add";
        uint32_t key = espHomeFnv1a(objId);

        ProtoWriter w;
        w.writeStringField(1, objId);                        // object_id
        w.writeFixed32Field(2, key);                         // key
        w.writeStringField(3, name + " Add");                // name
        w.writeStringField(4, objId);                        // unique_id
        w.writeStringField(5, "mdi:plus-circle-outline");    // icon
        w.writeVarintField(6, 1);                            // entity_category = 1 (config)

        sendFrame(sock, EspHomeMsg::LIST_ENTITIES_BUTTON_RESPONSE, w);
    }

    // 5. Remove Button
    {
        std::string objId = "cover_" + hexId + "_remove";
        uint32_t key = espHomeFnv1a(objId);

        ProtoWriter w;
        w.writeStringField(1, objId);                        // object_id
        w.writeFixed32Field(2, key);                         // key
        w.writeStringField(3, name + " Remove");             // name
        w.writeStringField(4, objId);                        // unique_id
        w.writeStringField(5, "mdi:minus-circle-outline");   // icon
        w.writeVarintField(6, 1);                            // entity_category = 1 (config)

        sendFrame(sock, EspHomeMsg::LIST_ENTITIES_BUTTON_RESPONSE, w);
    }
}

void handleListEntitiesRequest(int sock) {
    // 1. Send all configured cover remotes
    const auto &remotes = IOHC::iohcRemote1W::getInstance()->getRemotes();
    for (const auto &r : remotes) {
        std::string hexId = bytesToHexString(r.node, sizeof(r.node));
        std::string name = r.name.empty() ? r.description : r.name;
        sendCoverEntityList(sock, hexId, name);
    }

    // 2. Gateway Diagnostic Sensors
    // WiFi RSSI
    {
        std::string objId = "omni_wifi_rssi";
        uint32_t key = espHomeFnv1a(objId);

        ProtoWriter w;
        w.writeStringField(1, objId);                // object_id
        w.writeFixed32Field(2, key);                 // key
        w.writeStringField(3, "WiFi RSSI");          // name
        w.writeStringField(4, objId);                // unique_id
        w.writeStringField(5, "mdi:wifi");           // icon
        w.writeStringField(6, "dBm");                // unit_of_measurement
        w.writeVarintField(7, 0);                    // accuracy_decimals = 0
        w.writeStringField(9, "signal_strength");    // device_class
        w.writeVarintField(10, 1);                   // state_class = 1 (measurement)
        w.writeVarintField(11, 2);                   // entity_category = 2 (diagnostic)

        sendFrame(sock, EspHomeMsg::LIST_ENTITIES_SENSOR_RESPONSE, w);
    }

    // Free Heap Memory
    {
        std::string objId = "omni_free_heap";
        uint32_t key = espHomeFnv1a(objId);

        ProtoWriter w;
        w.writeStringField(1, objId);                // object_id
        w.writeFixed32Field(2, key);                 // key
        w.writeStringField(3, "Free Memory");        // name
        w.writeStringField(4, objId);                // unique_id
        w.writeStringField(5, "mdi:memory");         // icon
        w.writeStringField(6, "B");                  // unit_of_measurement
        w.writeVarintField(7, 0);                    // accuracy_decimals = 0
        w.writeStringField(9, "data_size");          // device_class
        w.writeVarintField(10, 1);                   // state_class = 1 (measurement)
        w.writeVarintField(11, 2);                   // entity_category = 2 (diagnostic)

        sendFrame(sock, EspHomeMsg::LIST_ENTITIES_SENSOR_RESPONSE, w);
    }

    // IP Address (Text Sensor)
    {
        std::string objId = "omni_ip_address";
        uint32_t key = espHomeFnv1a(objId);

        ProtoWriter w;
        w.writeStringField(1, objId);                // object_id
        w.writeFixed32Field(2, key);                 // key
        w.writeStringField(3, "IP Address");         // name
        w.writeStringField(4, objId);                // unique_id
        w.writeStringField(5, "mdi:ip-network");     // icon
        w.writeVarintField(6, 2);                    // entity_category = 2 (diagnostic)

        sendFrame(sock, EspHomeMsg::LIST_ENTITIES_TEXT_SENSOR_RESPONSE, w);
    }

    // 3. Finish list
    ProtoWriter done;
    sendFrame(sock, EspHomeMsg::LIST_ENTITIES_DONE_RESPONSE, done);
}

void dumpInitialStates(int sock) {
    // Covers & Travel Times
    const auto &remotes = IOHC::iohcRemote1W::getInstance()->getRemotes();
    for (const auto &r : remotes) {
        std::string hexId = bytesToHexString(r.node, sizeof(r.node));
        float posPct = r.positionTracker.getPosition();
        float posNorm = std::clamp(posPct / 100.0f, 0.0f, 1.0f);

        uint32_t currentOp = 0; // IDLE
        if (r.movement == IOHC::iohcRemote1W::remote::Movement::Opening) currentOp = 1;
        else if (r.movement == IOHC::iohcRemote1W::remote::Movement::Closing) currentOp = 2;

        uint32_t coverKey = espHomeFnv1a("cover_" + hexId);
        ProtoWriter cw;
        cw.writeFixed32Field(1, coverKey);
        cw.writeVarintField(2, posNorm > 0.05f ? 0 : 1); // legacy_state: 0=OPEN, 1=CLOSED
        cw.writeFloatField(3, posNorm);
        cw.writeFloatField(4, 0.0f);
        cw.writeVarintField(5, currentOp);
        sendFrame(sock, EspHomeMsg::COVER_STATE_RESPONSE, cw);

        // Travel time state
        uint32_t ttKey = espHomeFnv1a("cover_" + hexId + "_travel_time");
        ProtoWriter tw;
        tw.writeFixed32Field(1, ttKey);
        tw.writeFloatField(2, static_cast<float>(r.travelTime));
        sendFrame(sock, EspHomeMsg::NUMBER_STATE_RESPONSE, tw);
    }

    // Diagnostics
    {
        uint32_t rssiKey = espHomeFnv1a("omni_wifi_rssi");
        ProtoWriter w;
        w.writeFixed32Field(1, rssiKey);
        w.writeFloatField(2, static_cast<float>(WiFi.RSSI()));
        sendFrame(sock, EspHomeMsg::SENSOR_STATE_RESPONSE, w);
    }
    {
        uint32_t heapKey = espHomeFnv1a("omni_free_heap");
        ProtoWriter w;
        w.writeFixed32Field(1, heapKey);
        w.writeFloatField(2, static_cast<float>(esp_get_free_heap_size()));
        sendFrame(sock, EspHomeMsg::SENSOR_STATE_RESPONSE, w);
    }
    {
        uint32_t ipKey = espHomeFnv1a("omni_ip_address");
        ProtoWriter w;
        w.writeFixed32Field(1, ipKey);
        w.writeStringField(2, WiFi.localIP().toString().c_str());
        sendFrame(sock, EspHomeMsg::TEXT_SENSOR_STATE_RESPONSE, w);
    }
}

void handleCoverCommand(ProtoReader &reader) {
    uint32_t key = 0;
    bool hasLegacyCmd = false;
    uint32_t legacyCmd = 0; // 0=OPEN, 1=CLOSE, 2=STOP
    bool hasPosition = false;
    float position = 0.0f;
    bool stop = false;

    while (reader.hasNext()) {
        uint32_t field;
        uint8_t wireType;
        if (!reader.readTag(field, wireType)) break;

        if (field == 1 && wireType == 5) {
            reader.readFixed32(key);
        } else if (field == 2 && wireType == 0) {
            uint64_t v = 0;
            reader.readVarint(v);
            hasLegacyCmd = (v != 0);
        } else if (field == 3 && wireType == 0) {
            uint64_t v = 0;
            reader.readVarint(v);
            legacyCmd = static_cast<uint32_t>(v);
        } else if (field == 4 && wireType == 0) {
            uint64_t v = 0;
            reader.readVarint(v);
            hasPosition = (v != 0);
        } else if (field == 5 && wireType == 5) {
            reader.readFloat(position);
        } else if (field == 8 && wireType == 0) {
            uint64_t v = 0;
            reader.readVarint(v);
            stop = (v != 0);
        } else {
            reader.skipField(wireType);
        }
    }

    if (key == 0) return;

    // Find remote matching key
    const auto &remotes = IOHC::iohcRemote1W::getInstance()->getRemotes();
    for (const auto &r : remotes) {
        std::string hexId = bytesToHexString(r.node, sizeof(r.node));
        if (espHomeFnv1a("cover_" + hexId) == key) {
            Tokens t;
            if (stop || (hasLegacyCmd && legacyCmd == 2)) {
                t.push_back("stop");
                t.push_back(r.description);
                IOHC::iohcRemote1W::getInstance()->cmd(IOHC::RemoteButton::Stop, &t);
                addLogMessage(String("ESPHome cover STOP command: ") + r.description.c_str());
            } else if (hasPosition) {
                int openPct = static_cast<int>(std::round(position * 100.0f));
                openPct = std::clamp(openPct, 0, 100);
                int closeVal = 100 - openPct; // io-homecontrol 0=open, 100=closed
                t.push_back(std::to_string(closeVal));
                t.push_back(r.description);
                IOHC::iohcRemote1W::getInstance()->cmd(IOHC::RemoteButton::Absolute, &t);
                addLogMessage(String("ESPHome cover position: ") + r.description.c_str() + " -> " + String(openPct) + "%");
            } else if (hasLegacyCmd && legacyCmd == 0) {
                t.push_back("open");
                t.push_back(r.description);
                IOHC::iohcRemote1W::getInstance()->cmd(IOHC::RemoteButton::Open, &t);
                addLogMessage(String("ESPHome cover OPEN command: ") + r.description.c_str());
            } else if (hasLegacyCmd && legacyCmd == 1) {
                t.push_back("close");
                t.push_back(r.description);
                IOHC::iohcRemote1W::getInstance()->cmd(IOHC::RemoteButton::Close, &t);
                addLogMessage(String("ESPHome cover CLOSE command: ") + r.description.c_str());
            }
            break;
        }
    }
}

void handleButtonCommand(ProtoReader &reader) {
    uint32_t key = 0;
    while (reader.hasNext()) {
        uint32_t field;
        uint8_t wireType;
        if (!reader.readTag(field, wireType)) break;
        if (field == 1 && wireType == 5) {
            reader.readFixed32(key);
        } else {
            reader.skipField(wireType);
        }
    }

    if (key == 0) return;

    const auto &remotes = IOHC::iohcRemote1W::getInstance()->getRemotes();
    for (const auto &r : remotes) {
        std::string hexId = bytesToHexString(r.node, sizeof(r.node));
        if (espHomeFnv1a("cover_" + hexId + "_pair") == key) {
            Tokens t;
            t.push_back("pair");
            t.push_back(r.description);
            IOHC::iohcRemote1W::getInstance()->cmd(IOHC::RemoteButton::Pair, &t);
            addLogMessage(String("ESPHome pair button: ") + r.description.c_str());
            break;
        } else if (espHomeFnv1a("cover_" + hexId + "_add") == key) {
            Tokens t;
            t.push_back("add");
            t.push_back(r.description);
            IOHC::iohcRemote1W::getInstance()->cmd(IOHC::RemoteButton::Pair, &t);
            addLogMessage(String("ESPHome add button: ") + r.description.c_str());
            break;
        } else if (espHomeFnv1a("cover_" + hexId + "_remove") == key) {
            Tokens t;
            t.push_back("remove");
            t.push_back(r.description);
            IOHC::iohcRemote1W::getInstance()->cmd(IOHC::RemoteButton::Pair, &t);
            addLogMessage(String("ESPHome remove button: ") + r.description.c_str());
            break;
        }
    }
}

void handleNumberCommand(ProtoReader &reader) {
    uint32_t key = 0;
    float state = 0.0f;

    while (reader.hasNext()) {
        uint32_t field;
        uint8_t wireType;
        if (!reader.readTag(field, wireType)) break;
        if (field == 1 && wireType == 5) {
            reader.readFixed32(key);
        } else if (field == 2 && wireType == 5) {
            reader.readFloat(state);
        } else {
            reader.skipField(wireType);
        }
    }

    if (key == 0) return;

    const auto &remotes = IOHC::iohcRemote1W::getInstance()->getRemotes();
    for (const auto &r : remotes) {
        std::string hexId = bytesToHexString(r.node, sizeof(r.node));
        if (espHomeFnv1a("cover_" + hexId + "_travel_time") == key) {
            uint32_t tt = static_cast<uint32_t>(std::max(0.0f, state));
            IOHC::iohcRemote1W::getInstance()->setTravelTime(r.description, tt);
            addLogMessage(String("ESPHome travel time set: ") + r.description.c_str() + " -> " + String(tt) + "s");

            // Broadcast back new state
            ProtoWriter w;
            w.writeFixed32Field(1, key);
            w.writeFloatField(2, static_cast<float>(tt));
            broadcastFrameToSubscribers(EspHomeMsg::NUMBER_STATE_RESPONSE, w);
            break;
        }
    }
}

// ==================== DISPATCHER ====================

void dispatchMessage(ClientSession &session, uint16_t msgType, const uint8_t *payload, size_t payloadLen) {
    session.lastActivityMs = millis();
    ProtoReader reader(payload, payloadLen);

    switch (msgType) {
        case EspHomeMsg::HELLO_REQUEST:
            handleHelloRequest(session.socketFd, reader);
            break;

        case EspHomeMsg::CONNECT_REQUEST:
            handleConnectRequest(session.socketFd, reader, session);
            break;

        case EspHomeMsg::PING_REQUEST: {
            ProtoWriter writer;
            sendFrame(session.socketFd, EspHomeMsg::PING_RESPONSE, writer);
            break;
        }

        case EspHomeMsg::DISCONNECT_REQUEST: {
            ProtoWriter writer;
            sendFrame(session.socketFd, EspHomeMsg::DISCONNECT_RESPONSE, writer);
            close(session.socketFd);
            session.socketFd = -1;
            break;
        }

        case EspHomeMsg::DEVICE_INFO_REQUEST:
            handleDeviceInfoRequest(session.socketFd);
            break;

        case EspHomeMsg::LIST_ENTITIES_REQUEST:
            handleListEntitiesRequest(session.socketFd);
            break;

        case EspHomeMsg::SUBSCRIBE_STATES_REQUEST:
            session.subscribedStates = true;
            dumpInitialStates(session.socketFd);
            break;

        case EspHomeMsg::COVER_COMMAND_REQUEST:
            handleCoverCommand(reader);
            break;

        case EspHomeMsg::BUTTON_COMMAND_REQUEST:
            handleButtonCommand(reader);
            break;

        case EspHomeMsg::NUMBER_COMMAND_REQUEST:
            handleNumberCommand(reader);
            break;

        case EspHomeMsg::SUBSCRIBE_LOGS_REQUEST:
        case EspHomeMsg::SUBSCRIBE_HOMEASSISTANT_SERVICES_REQUEST:
        case EspHomeMsg::SUBSCRIBE_HOME_ASSISTANT_STATES_REQUEST:
            // Optional/ignorable subscriptions in native API
            break;

        default:
            // Unknown or unhandled message; skip silently for forward compatibility
            break;
    }
}

// ==================== SESSION THREAD & SERVER TASK ====================

void processClientSession(ClientSession &session) {
    uint8_t headerBuf[16];

    // 1. Read preamble (1 byte)
    ssize_t n = recv(session.socketFd, headerBuf, 1, MSG_DONTWAIT);
    if (n <= 0) {
        if (n == 0) {
            // EOF: peer closed
            close(session.socketFd);
            session.socketFd = -1;
        }
        return;
    }

    uint8_t preamble = headerBuf[0];
    if (preamble != 0x00) {
        // Not a plaintext frame (or unrecognized preamble)
        Serial.printf("ESPHome: Bad preamble 0x%02x, closing session\n", preamble);
        close(session.socketFd);
        session.socketFd = -1;
        return;
    }

    // 2. Read length varint
    uint64_t length = 0;
    uint32_t shift = 0;
    while (true) {
        uint8_t b;
        if (recv(session.socketFd, &b, 1, 0) <= 0) {
            close(session.socketFd);
            session.socketFd = -1;
            return;
        }
        length |= static_cast<uint64_t>(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
        if (shift >= 64) {
            close(session.socketFd);
            session.socketFd = -1;
            return;
        }
    }

    // 3. Read message type varint
    uint64_t rawType = 0;
    shift = 0;
    while (true) {
        uint8_t b;
        if (recv(session.socketFd, &b, 1, 0) <= 0) {
            close(session.socketFd);
            session.socketFd = -1;
            return;
        }
        rawType |= static_cast<uint64_t>(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
        if (shift >= 64) {
            close(session.socketFd);
            session.socketFd = -1;
            return;
        }
    }

    if (length > 16384) {
        // Sanity limit to avoid memory exhaustion
        Serial.printf("ESPHome: Frame length %llu too large\n", length);
        close(session.socketFd);
        session.socketFd = -1;
        return;
    }

    // 4. Read payload
    std::vector<uint8_t> payload(length);
    size_t received = 0;
    while (received < length) {
        ssize_t chunk = recv(session.socketFd, payload.data() + received, length - received, 0);
        if (chunk <= 0) {
            close(session.socketFd);
            session.socketFd = -1;
            return;
        }
        received += chunk;
    }

    dispatchMessage(session, static_cast<uint16_t>(rawType), payload.data(), payload.size());
}

void esphomeServerTask(void *param) {
    (void)param;
    Serial.println("ESPHome: Server task started");

    while (s_serverRunning) {
        if (s_serverSocket < 0) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        fd_set readFds;
        FD_ZERO(&readFds);
        FD_SET(s_serverSocket, &readFds);
        int maxFd = s_serverSocket;

        {
            std::lock_guard<std::recursive_mutex> lock(s_clientsMutex);
            for (size_t i = 0; i < MAX_CLIENTS; i++) {
                if (s_clients[i].socketFd >= 0) {
                    FD_SET(s_clients[i].socketFd, &readFds);
                    if (s_clients[i].socketFd > maxFd) {
                        maxFd = s_clients[i].socketFd;
                    }
                }
            }
        }

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms timeout

        int activity = select(maxFd + 1, &readFds, nullptr, nullptr, &tv);
        if (activity < 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // Accept new connections
        if (FD_ISSET(s_serverSocket, &readFds)) {
            struct sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            int newSock = accept(s_serverSocket, reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);
            if (newSock >= 0) {
                // Set TCP_NODELAY and socket timeouts (3s) to prevent hanging
                int flag = 1;
                setsockopt(newSock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&flag), sizeof(int));

                struct timeval tvTimeout;
                tvTimeout.tv_sec = 3;
                tvTimeout.tv_usec = 0;
                setsockopt(newSock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&tvTimeout), sizeof(tvTimeout));
                setsockopt(newSock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char*>(&tvTimeout), sizeof(tvTimeout));

                std::lock_guard<std::recursive_mutex> lock(s_clientsMutex);
                bool slotFound = false;
                for (size_t i = 0; i < MAX_CLIENTS; i++) {
                    if (s_clients[i].socketFd < 0) {
                        s_clients[i].socketFd = newSock;
                        s_clients[i].authenticated = false;
                        s_clients[i].subscribedStates = false;
                        s_clients[i].lastActivityMs = millis();
                        slotFound = true;
                        char ipStr[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(clientAddr.sin_addr), ipStr, INET_ADDRSTRLEN);
                        Serial.printf("ESPHome: Client connected from %s (slot %d)\n", ipStr, static_cast<int>(i));
                        addLogMessage(String("ESPHome client connected: ") + ipStr);
                        break;
                    }
                }
                if (!slotFound) {
                    Serial.println("ESPHome: Max clients reached, rejecting");
                    close(newSock);
                }
            }
        }

        // Process data from active clients
        {
            std::lock_guard<std::recursive_mutex> lock(s_clientsMutex);
            for (size_t i = 0; i < MAX_CLIENTS; i++) {
                if (s_clients[i].socketFd >= 0) {
                    if (FD_ISSET(s_clients[i].socketFd, &readFds)) {
                        processClientSession(s_clients[i]);
                    }

                    // Keepalive check: if no activity for 60s, close
                    if (s_clients[i].socketFd >= 0 && millis() - s_clients[i].lastActivityMs > 90000UL) {
                        Serial.printf("ESPHome: Client %d silent timeout\n", static_cast<int>(i));
                        close(s_clients[i].socketFd);
                        s_clients[i].socketFd = -1;
                    }
                }
            }
        }
    }

    // Cleanup when stopping
    {
        std::lock_guard<std::recursive_mutex> lock(s_clientsMutex);
        for (size_t i = 0; i < MAX_CLIENTS; i++) {
            if (s_clients[i].socketFd >= 0) {
                close(s_clients[i].socketFd);
                s_clients[i].socketFd = -1;
            }
        }
    }
    if (s_serverSocket >= 0) {
        close(s_serverSocket);
        s_serverSocket = -1;
    }
    Serial.println("ESPHome: Server task finished");
    s_serverTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

} // anonymous namespace

// ==================== PUBLIC API IMPLEMENTATION ====================

void initEspHomeServer() {
    bool en = true;
    if (nvs_read_bool(NVS_KEY_ESPHOME_EN, en)) {
        esphome_enabled = en;
    }
    uint16_t port = 6053;
    if (nvs_read_u16(NVS_KEY_ESPHOME_PORT, port)) {
        esphome_port = port;
    }
    std::string pwd;
    if (nvs_read_string(NVS_KEY_ESPHOME_PWD, pwd)) {
        esphome_password = pwd;
    }
    std::string name;
    if (nvs_read_string(NVS_KEY_ESPHOME_NAME, name)) {
        esphome_name = name;
    }

    std::lock_guard<std::recursive_mutex> lock(s_clientsMutex);
    for (size_t i = 0; i < MAX_CLIENTS; i++) {
        s_clients[i].socketFd = -1;
    }
}

void startEspHomeServer() {
    if (!esphome_enabled) {
        Serial.println("ESPHome: Disabled in config");
        return;
    }
    if (s_serverRunning) return;

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(esphome_port);

    s_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (s_serverSocket < 0) {
        Serial.println("ESPHome: Failed to create socket");
        return;
    }

    int opt = 1;
    setsockopt(s_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(s_serverSocket, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        Serial.printf("ESPHome: Failed to bind port %u\n", esphome_port);
        close(s_serverSocket);
        s_serverSocket = -1;
        return;
    }

    if (listen(s_serverSocket, 4) < 0) {
        Serial.println("ESPHome: Failed to listen on socket");
        close(s_serverSocket);
        s_serverSocket = -1;
        return;
    }

    s_serverRunning = true;
    xTaskCreatePinnedToCore(esphomeServerTask, "esphome_api", 4096, nullptr, 2, &s_serverTaskHandle, 1);
    Serial.printf("ESPHome: Native API server listening on port %u\n", esphome_port);
    addLogMessage(String("ESPHome Native API server listening on port ") + String(esphome_port));
}

void stopEspHomeServer() {
    if (!s_serverRunning) return;
    s_serverRunning = false;
    if (s_serverSocket >= 0) {
        close(s_serverSocket);
        s_serverSocket = -1;
    }
    // Server task will close client sockets and delete itself
}

bool isEspHomeServerRunning() {
    return s_serverRunning;
}

uint16_t getEspHomeConnectedClients() {
    std::lock_guard<std::recursive_mutex> lock(s_clientsMutex);
    uint16_t count = 0;
    for (size_t i = 0; i < MAX_CLIENTS; i++) {
        if (s_clients[i].socketFd >= 0 && s_clients[i].authenticated) {
            count++;
        }
    }
    return count;
}

void notifyEspHomeCoverPosition(const std::string &id, float position) {
    if (!s_serverRunning) return;

    std::string lowerId = id;
    std::transform(lowerId.begin(), lowerId.end(), lowerId.begin(), ::tolower);
    uint32_t coverKey = espHomeFnv1a("cover_" + lowerId);

    float posNorm = std::clamp(position / 100.0f, 0.0f, 1.0f);

    uint32_t currentOp = 0;
    const auto &remotes = IOHC::iohcRemote1W::getInstance()->getRemotes();
    auto it = std::find_if(remotes.begin(), remotes.end(), [&](const auto &r) {
        return bytesToHexString(r.node, sizeof(r.node)) == lowerId;
    });
    if (it != remotes.end()) {
        if (it->movement == IOHC::iohcRemote1W::remote::Movement::Opening) currentOp = 1;
        else if (it->movement == IOHC::iohcRemote1W::remote::Movement::Closing) currentOp = 2;
    }

    ProtoWriter cw;
    cw.writeFixed32Field(1, coverKey);
    cw.writeVarintField(2, posNorm > 0.05f ? 0 : 1); // legacy_state: 0=OPEN, 1=CLOSED
    cw.writeFloatField(3, posNorm);
    cw.writeFloatField(4, 0.0f);
    cw.writeVarintField(5, currentOp);

    broadcastFrameToSubscribers(EspHomeMsg::COVER_STATE_RESPONSE, cw);
}

void notifyEspHomeCoverState(const std::string &id, const char *state) {
    if (!s_serverRunning) return;

    std::string lowerId = id;
    std::transform(lowerId.begin(), lowerId.end(), lowerId.begin(), ::tolower);
    uint32_t coverKey = espHomeFnv1a("cover_" + lowerId);

    uint32_t currentOp = 0; // IDLE
    if (strcmp(state, "OPENING") == 0) currentOp = 1;
    else if (strcmp(state, "CLOSING") == 0) currentOp = 2;

    float posNorm = 0.5f;
    const auto &remotes = IOHC::iohcRemote1W::getInstance()->getRemotes();
    auto it = std::find_if(remotes.begin(), remotes.end(), [&](const auto &r) {
        return bytesToHexString(r.node, sizeof(r.node)) == lowerId;
    });
    if (it != remotes.end()) {
        posNorm = std::clamp(it->positionTracker.getPosition() / 100.0f, 0.0f, 1.0f);
    }

    ProtoWriter cw;
    cw.writeFixed32Field(1, coverKey);
    cw.writeVarintField(2, posNorm > 0.05f ? 0 : 1);
    cw.writeFloatField(3, posNorm);
    cw.writeFloatField(4, 0.0f);
    cw.writeVarintField(5, currentOp);

    broadcastFrameToSubscribers(EspHomeMsg::COVER_STATE_RESPONSE, cw);
}

void notifyEspHomeDiagnostics() {
    if (!s_serverRunning) return;

    {
        uint32_t rssiKey = espHomeFnv1a("omni_wifi_rssi");
        ProtoWriter w;
        w.writeFixed32Field(1, rssiKey);
        w.writeFloatField(2, static_cast<float>(WiFi.RSSI()));
        broadcastFrameToSubscribers(EspHomeMsg::SENSOR_STATE_RESPONSE, w);
    }
    {
        uint32_t heapKey = espHomeFnv1a("omni_free_heap");
        ProtoWriter w;
        w.writeFixed32Field(1, heapKey);
        w.writeFloatField(2, static_cast<float>(esp_get_free_heap_size()));
        broadcastFrameToSubscribers(EspHomeMsg::SENSOR_STATE_RESPONSE, w);
    }
}

void syncEspHomeDevices() {
    // If device list was changed (added, removed, renamed), notify connected clients by re-dumping
    if (!s_serverRunning) return;

    std::lock_guard<std::recursive_mutex> lock(s_clientsMutex);
    for (size_t i = 0; i < MAX_CLIENTS; i++) {
        if (s_clients[i].socketFd >= 0 && s_clients[i].authenticated) {
            handleListEntitiesRequest(s_clients[i].socketFd);
            if (s_clients[i].subscribedStates) {
                dumpInitialStates(s_clients[i].socketFd);
            }
        }
    }
}

#endif // ESPHOME_API
