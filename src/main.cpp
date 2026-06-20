/*
   Copyright (c) 2024. CRIDP https://github.com/cridp

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

           http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */

#include "esp_log.h"
#include "esp_system.h"
#include <board-config.h>
#include <user_config.h>

#include <crypto2Wutils.h>
#include <iohcCryptoHelpers.h>
#include <iohcRadio.h>

#include <iohcSystemTable.h>
#include <fileSystemHelpers.h>
#include <ArduinoJson.h>
#include <iohcRemote1W.h>
#include <iohcCozyDevice2W.h>
#include <iohcOtherDevice2W.h>
#include <iohcRemoteMap.h>
#include <interact.h>
#if defined(MQTT)
#include <mqtt_handler.h>
#endif
#include <wifi_helper.h>
#include <nvs_helpers.h>
#include "log_buffer.h"
#include <stdarg.h>
#include <algorithm>
#include <array>
#include <cstring>

#if defined(WEBSERVER)
#include <web_server_handler.h>
#endif
#include "LittleFS.h"
//#include <WiFi.h> // Assuming WiFi is used and initialized elsewhere or will be here.


#include <user_config.h>
#include <oled_display.h>


extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

void txUserBuffer(Tokens *cmd);
void testKey();
void scanDump();
bool publishMsg(IOHC::iohcPacket *iohc);
bool msgRcvd(IOHC::iohcPacket *iohc);
bool msgArchive(IOHC::iohcPacket *iohc);
void IRAM_ATTR forgePacket(IOHC::iohcPacket *packet, const std::vector<uint8_t> &toSend);



uint8_t keyCap[16] = {};
//uint8_t source_originator[3] = {0};

IOHC::iohcRadio *radioInstance;
IOHC::iohcPacket *radioPackets[IOHC_INBOUND_MAX_PACKETS];

uint8_t nextPacket = 0;

IOHC::iohcSystemTable *sysTable;
IOHC::iohcRemote1W *remote1W;
IOHC::iohcCozyDevice2W *cozyDevice2W;
IOHC::iohcOtherDevice2W *otherDevice2W;
IOHC::iohcRemoteMap *remoteMap;
static uint32_t lastTwoWLearnDiscoverMs = 0;

enum class TwoWPairStage : uint8_t {
    Idle,
    AwaitDiscoverAnswer,
    AwaitActuatorAck,
    AwaitKeyTransfer,
    AwaitKeyAuthentication
};

struct TwoWPairSession {
    TwoWPairStage stage = TwoWPairStage::Idle;
    IOHC::address peer = {};
    uint32_t frequency = CHANNEL2;
    std::array<uint8_t, 9> metadata = {};
    std::array<uint8_t, 6> transferChallenge = {};
    std::array<uint8_t, 6> authenticationChallenge = {};
    std::array<uint8_t, 17> transferredFrameData = {};
    std::array<std::array<uint8_t, 16>, 2> candidateKeys = {};
};

static TwoWPairSession twoWPairSession;

uint32_t frequencies[] = FREQS2SCAN;
constexpr uint8_t kNumScanFrequencies =
    static_cast<uint8_t>(sizeof(frequencies) / sizeof(frequencies[0]));

using namespace IOHC;

static const char *resetReasonName(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON: return "poweron";
        case ESP_RST_EXT: return "external";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "interrupt_wdt";
        case ESP_RST_TASK_WDT: return "task_wdt";
        case ESP_RST_WDT: return "watchdog";
        case ESP_RST_DEEPSLEEP: return "deepsleep";
        case ESP_RST_BROWNOUT: return "brownout";
        case ESP_RST_SDIO: return "sdio";
        default: return "unknown";
    }
}

void resetTwoWPairingSession() {
    twoWPairSession = {};
    twoWPairSession.stage = Cmd::pairMode
        ? TwoWPairStage::AwaitDiscoverAnswer
        : TwoWPairStage::Idle;
}

static bool isPairingPeer(const IOHC::iohcPacket *packet) {
    return packet &&
           memcmp(twoWPairSession.peer, packet->payload.packet.header.source,
                  sizeof(IOHC::address)) == 0;
}

static IOHC::iohcPacket *makeTwoWPairPacket(
    uint8_t command, const std::vector<uint8_t> &data,
    const IOHC::address target, uint32_t frequency,
    bool startFrame, bool endFrame, bool acknowledgement) {
    auto *packet = new IOHC::iohcPacket;
    forgePacket(packet, data);
    packet->payload.packet.header.cmd = command;
    packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = startFrame;
    packet->payload.packet.header.CtrlByte1.asStruct.EndFrame = endFrame;
    packet->payload.packet.header.CtrlByte2.asByte = 0;
    packet->payload.packet.header.CtrlByte2.asStruct.Prio = acknowledgement;
    memcpy(packet->payload.packet.header.source, otherDevice2W->gateway, 3);
    memcpy(packet->payload.packet.header.target, target, 3);
    packet->frequency = frequency;
    packet->repeatTime = 50;
    return packet;
}

static void fillRandom(std::array<uint8_t, 6> &value) {
    esp_fill_random(value.data(), value.size());
}

static std::array<uint8_t, 16> decryptTransferredKey(
    const uint8_t *encryptedKey, uint8_t ivCommand) {
    std::vector<uint8_t> frameData = {ivCommand};
    std::vector<uint8_t> challenge(twoWPairSession.transferChallenge.begin(),
                                   twoWPairSession.transferChallenge.end());
    uint8_t initialValue[16] = {};
    constructInitialValue(frameData, initialValue, frameData.size(), challenge, nullptr);
    AES_init_ctx(&ctx, transfert_key);
    AES_ECB_encrypt(&ctx, initialValue);

    std::array<uint8_t, 16> result = {};
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] = encryptedKey[i] ^ initialValue[i];
    }
    return result;
}

static bool authenticateTransferredKey(
    const uint8_t *answer, const std::array<uint8_t, 16> &key) {
    std::vector<uint8_t> frameData(twoWPairSession.transferredFrameData.begin(),
                                   twoWPairSession.transferredFrameData.end());
    std::vector<uint8_t> challenge(twoWPairSession.authenticationChallenge.begin(),
                                   twoWPairSession.authenticationChallenge.end());
    uint8_t initialValue[16] = {};
    constructInitialValue(frameData, initialValue, frameData.size(), challenge, nullptr);
    AES_init_ctx(&ctx, key.data());
    AES_ECB_encrypt(&ctx, initialValue);
    return memcmp(initialValue, answer, 6) == 0;
}

static void persistTwoWSystemKey(const std::array<uint8_t, 16> &key) {
    memcpy(system_key, key.data(), key.size());
    nvs_write_string(NVS_KEY_2W_SYSTEM,
                     bytesToHexString(key.data(), static_cast<uint8_t>(key.size())));
}

static void loadTwoWSystemKey() {
    std::string encoded;
    uint8_t storedKey[16] = {};
    if (nvs_read_string(NVS_KEY_2W_SYSTEM, encoded) &&
        hexStringToBytes(encoded, storedKey) == sizeof(storedKey)) {
        memcpy(system_key, storedKey, sizeof(storedKey));
        addLogMessage("Stored 2W system key loaded");
    }
}

// Custom log vprintf that also stores to buffer
int log_to_buffer_and_serial(const char *format, va_list args) {
    char buf[256];
    vsnprintf(buf, sizeof(buf), format, args); // Format naar buffer
    return Serial.printf("%s", buf);
}

void setup() {

    Serial.begin(115200);       //Start serial connection for debug and manual input
    esp_log_set_vprintf(log_to_buffer_and_serial);
    esp_log_level_set("*", ESP_LOG_WARN);
    ESP_LOGW("SETUP", "START OF SETUP");
    addLogMessage(String("Boot reset reason: ") + resetReasonName(esp_reset_reason()));
    const String crashMarker = getCrashMarker();
    if (!crashMarker.isEmpty()) {
        addLogMessage("Last crash marker: " + crashMarker);
    }

    initDisplay(); // Init OLED display

    pinMode(RX_LED, OUTPUT); // Blink this LED
    digitalWrite(RX_LED, 1);

    // Mount LittleFS filesystem
#if defined(ESP32)
    // LittleFS.begin(); // Original call, replaced by new init below
    if(!LittleFS.begin()){
        Serial.println("An Error has occurred while mounting LittleFS");
        // Handle error appropriately, maybe by halting or indicating failure
        return;
    }
    Serial.println("LittleFS mounted successfully");
#endif
    nvs_init();
    loadTwoWSystemKey();

    // Load 1W device definitions before starting network services so
    // that /api/devices can immediately return the configured remotes.
    remote1W = IOHC::iohcRemote1W::getInstance();

    radioInstance = IOHC::iohcRadio::getInstance();
    radioInstance->start(kNumScanFrequencies, frequencies, 0, msgRcvd,
                         publishMsg); //msgArchive); //, msgRcvd);

    sysTable = IOHC::iohcSystemTable::getInstance();

    cozyDevice2W = IOHC::iohcCozyDevice2W::getInstance();
    otherDevice2W = IOHC::iohcOtherDevice2W::getInstance();
    remoteMap = IOHC::iohcRemoteMap::getInstance();

    //   AES_init_ctx(&ctx, transfert_key); // PreInit AES for cozy (1W use original version) TODO

    Cmd::createCommands();

    // Initialize network services after devices are ready
    initWifi();
#if defined(MQTT)
    initMqtt();
#endif
    Cmd::kbd_tick.attach_ms(500, Cmd::cmdFuncHandler);

//    esp_timer_dump(stdout);

    printf("Startup completed. type help to see what you can do!\n");
    digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
}

/**
 * The function `forgePacket` modifies a given `iohcPacket` structure with specific values and
 * settings.
 *
 * @param packet The `packet` parameter is a pointer to an `iohcPacket` struct.
 * @param toSend The `vector` parameter in the `forgePacket` function is a `std::vector<uint8_t>` type,
 * which is a standard C++ container that stores a sequence of elements of type `uint8_t` (unsigned
 * 8-bit integer). In this function, the size of the `
 */
/**
* @brief Creates a iohcPacket with the given data to send.
* @param packet * The packet you want to forge
* @param toSend The data that will be added to the packet
*/
void IRAM_ATTR forgePacket(iohcPacket* packet, const std::vector<uint8_t> &toSend) {
    digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
    IOHC::packetStamp.store(esp_timer_get_time());

    // Common Flags
    // 8 if protocol version is 0 else 10
    packet->payload.packet.header.CtrlByte1.asStruct.MsgLen = sizeof(_header) - 1;
    packet->payload.packet.header.CtrlByte1.asStruct.Protocol = 0;
    packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;
    packet->payload.packet.header.CtrlByte1.asStruct.EndFrame = 0;
    packet->payload.packet.header.CtrlByte1.asByte += toSend.size();
    memcpy(packet->payload.buffer + 9, toSend.data(), toSend.size());
    packet->buffer_length = toSend.size() + 9;

    packet->payload.packet.header.CtrlByte2.asByte = 0;

    packet->frequency = CHANNEL2;
    packet->repeatTime = 25;
    packet->repeat = 0;
    packet->lock = false;
}

bool msgRcvd(IOHC::iohcPacket *iohc) {
    JsonDocument doc;
    doc["type"] = "Unk";
#if defined(WEBSERVER)
    if (!iohc || iohc->buffer_length < sizeof(_header) || iohc->buffer_length > MAX_FRAME_LEN) {
        const uint8_t safeLen = iohc ? std::min<uint8_t>(iohc->buffer_length, MAX_FRAME_LEN) : 0;
        const uint8_t expectedLength = (iohc && safeLen > 0)
                                           ? iohc->payload.packet.header.CtrlByte1.asStruct.MsgLen + 1
                                           : 0;
        updateTwoWRxStatus(
            "RAW rejected",
            "-",
            "-",
            "-",
            "len=" + String(iohc ? iohc->buffer_length : 0) +
                " expected=" + String(expectedLength) +
                " raw=" + String(iohc ? bytesToHexString(iohc->payload.buffer, safeLen).c_str() : ""),
            iohc ? String(iohc->frequency) : ""
        );
        return true;
    }
#else
    if (!iohc || iohc->buffer_length < sizeof(_header) || iohc->buffer_length > MAX_FRAME_LEN) {
        return true;
    }
#endif
    IOHC::Address3 lastFrom{};
    memcpy(lastFrom.b, iohc->payload.packet.header.source, sizeof(lastFrom.b));
    IOHC::lastFromAddress.store(lastFrom);
#if defined(WEBSERVER)
    broadcastLastAddress(bytesToHexString(lastFrom.b, sizeof(lastFrom.b)).c_str());
    if (iohc->payload.packet.header.CtrlByte1.asStruct.Protocol == 0) {
        const uint8_t twoWExpectedLength = iohc->payload.packet.header.CtrlByte1.asStruct.MsgLen + 1;
        if (iohc->buffer_length < sizeof(_header) ||
            iohc->buffer_length > MAX_FRAME_LEN ||
            twoWExpectedLength < sizeof(_header) ||
            twoWExpectedLength > MAX_FRAME_LEN ||
            iohc->buffer_length != twoWExpectedLength) {
            updateTwoWRxStatus(
                "RAW rejected",
                "-",
                "-",
                "-",
                "len=" + String(iohc->buffer_length) +
                    " expected=" + String(twoWExpectedLength) +
                    " raw=" + String(bytesToHexString(iohc->payload.buffer, MAX_FRAME_LEN).c_str()),
                String(iohc->frequency)
            );
            return true;
        } else {
            const String twoWFrom = bytesToHexString(iohc->payload.packet.header.source, 3).c_str();
            const String twoWTo = bytesToHexString(iohc->payload.packet.header.target, 3).c_str();
            const String twoWCmd = to_hex_str(iohc->payload.packet.header.cmd).c_str();
            const uint8_t twoWDataLength = iohc->buffer_length > sizeof(_header) ? iohc->buffer_length - sizeof(_header) : 0;
            const uint8_t *twoWPayload = iohc->payload.buffer + sizeof(_header);
            const String twoWData = bytesToHexString(twoWPayload, twoWDataLength).c_str();
            const String twoWRaw = bytesToHexString(iohc->payload.buffer, iohc->buffer_length).c_str();
            const String twoWFullData = "len=" + String(iohc->buffer_length) +
                                        " payload=" + twoWData +
                                        " raw=" + twoWRaw;
            const uint8_t twoWGateway[3] = {0xba, 0x11, 0xad};
            if (memcmp(iohc->payload.packet.header.source, twoWGateway, sizeof(twoWGateway)) == 0) {
                updateTwoWRxStatus(
                    "2W self",
                    twoWFrom,
                    twoWTo,
                    twoWCmd,
                    twoWFullData,
                    String(iohc->frequency)
                );
                addLogMessage("2W self packet ignored cmd=" + twoWCmd + " to=" + twoWTo +
                              " data=" + twoWData + " raw=" + twoWRaw);
            } else {
                String twoWDecoded;
                if (iohc->payload.packet.header.cmd == IOHC::iohcDevice::RECEIVED_WRITE_PRIVATE_0x20 &&
                    twoWDataLength >= 5) {
                    const uint8_t twoWField = twoWPayload[3];
                    const uint8_t twoWValue = twoWPayload[4];
                    twoWDecoded = " origin=" + String(bytesToHexString(twoWPayload, 1).c_str()) +
                                  " acei=" + String(bytesToHexString(twoWPayload + 1, 1).c_str()) +
                                  " main=" + String(bytesToHexString(twoWPayload + 2, 1).c_str()) +
                                  " fp1=" + String(bytesToHexString(twoWPayload + 3, 1).c_str()) +
                                  " fp2=" + String(bytesToHexString(twoWPayload + 4, 1).c_str());
                    if (twoWPayload[0] == 0x0c &&
                        twoWPayload[1] == 0x61 &&
                        twoWPayload[2] == 0x01) {
                        switch (twoWField) {
                            case 0x00: {
                                const char *mode = "unknown";
                                if (twoWValue == 0x00) mode = "auto";
                                else if (twoWValue == 0x01) mode = "manual";
                                else if (twoWValue == 0x02) mode = "prog";
                                else if (twoWValue == 0x04) mode = "off";
                                twoWDecoded += " mode=" + String(mode);
                                break;
                            }
                            case 0x03:
                                twoWDecoded += " temp=" + String(twoWValue / 10.0f, 1);
                                break;
                            case 0x0e:
                                twoWDecoded += " window=" + String(twoWValue == 0x01 ? "open" : "closed");
                                break;
                            case 0x10:
                                twoWDecoded += " presence=" + String(twoWValue == 0x01 ? "on" : "off");
                                break;
                            default:
                                break;
                        }
                    }
                }
                updateTwoWRxStatus(
                    "2W",
                    twoWFrom,
                    twoWTo,
                    twoWCmd,
                    twoWDecoded.isEmpty() ? twoWFullData : twoWFullData + twoWDecoded,
                    String(iohc->frequency)
                );
                addLogMessage("2W RX cmd=" + twoWCmd + " from=" + twoWFrom +
                              " to=" + twoWTo + " data=" + twoWData +
                              " raw=" + twoWRaw + twoWDecoded);
            }
        }
    }
#endif
    String deviceId =
        bytesToHexString(iohc->payload.packet.header.source,
                         sizeof(iohc->payload.packet.header.source))
            .c_str();
    String deviceName = "Unknown device";
    const auto &remotes = IOHC::iohcRemote1W::getInstance()->getRemotes();
    auto rit = std::find_if(
        remotes.begin(), remotes.end(), [&](const auto &r) {
          return memcmp(r.node, iohc->payload.packet.header.source,
                        sizeof(r.node)) == 0;
        });
    if (rit != remotes.end()) {
      deviceName = rit->name.c_str();
    } else if (remoteMap) {
      const auto *entry = remoteMap->find(iohc->payload.packet.header.source);
      if (entry)
        deviceName = entry->name.c_str();
    }
    addLogMessage("Command received from " + deviceId +
                  " (" + deviceName + ")");
    switch (iohc->payload.packet.header.cmd) {
        case iohcDevice::RECEIVED_DISCOVER_0x28: {
            printf("2W Pairing Asked\n");
            if (!Cmd::pairMode) break;
            addLogMessage("2W pair step: received 0x28 discover; sending 0x29 discover answer");

            // 0x0b OverKiz 0x0c Atlantic
            std::vector<uint8_t> toSend = {0xff, 0xc0, 0xba, 0x11, 0xad, 0x0b, 0xcc, 0x00, 0x00};

            auto* packet = new iohcPacket;
            forgePacket(packet, toSend);

            packet->payload.packet.header.cmd = IOHC::iohcDevice::SEND_DISCOVER_ANSWER_0x29;

            /* Swap */
            memcpy(packet->payload.packet.header.source, cozyDevice2W->gateway, 3);
            memcpy(packet->payload.packet.header.target, iohc->payload.packet.header.source, 3);

            packet->delayed = 250;
            packet->repeat = 0;

            digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
            radioInstance->send(packet);
            break;
        }
        case iohcDevice::RECEIVED_DISCOVER_ANSWER_0x29: {
            printf("2W Device want to be paired\n");
            if (!Cmd::pairMode) break;
            if (iohc->buffer_length < 18) {
                addLogMessage("2W pair error: short 0x29 discovery answer");
                break;
            }
            addLogMessage("2W pair step: received 0x29 discover answer; sending 0x2C actuator discover");

            memcpy(twoWPairSession.peer, iohc->payload.packet.header.source, 3);
            memcpy(twoWPairSession.metadata.data(), iohc->payload.buffer + 9,
                   twoWPairSession.metadata.size());
            twoWPairSession.frequency = iohc->frequency;
            twoWPairSession.stage = TwoWPairStage::AwaitActuatorAck;

            auto *packet = makeTwoWPairPacket(
                iohcDevice::SEND_DISCOVER_ACTUATOR_0x2C, {},
                twoWPairSession.peer, twoWPairSession.frequency,
                true, false, false);
            radioInstance->send(packet);
            break;
        }
        case iohcDevice::RECEIVED_DISCOVER_REMOTE_ANSWER_0x2B: {
            sysTable->addObject(iohc->payload.packet.header.source, iohc->payload.packet.msg.p0x2b.backbone,
                                iohc->payload.packet.msg.p0x2b.actuator, iohc->payload.packet.msg.p0x2b.manufacturer,
                                iohc->payload.packet.msg.p0x2b.info);
            break;
        }
        case iohcDevice::RECEIVED_DISCOVER_ACTUATOR_0x2C: {
            printf("2W Actuator Ack Asked\n");
            if (!Cmd::pairMode) break;
            addLogMessage("2W pair step: received 0x2C actuator discover; sending 0x2D ack");

            std::vector<uint8_t> toSend = {};

            auto* packet = new iohcPacket;
            forgePacket(packet, toSend);

            packet->payload.packet.header.cmd = IOHC::iohcDevice::SEND_DISCOVER_ACTUATOR_ACK_0x2D;

            /* Swap */
            memcpy(packet->payload.packet.header.source, iohc->payload.packet.header.target, 3);
            memcpy(packet->payload.packet.header.target, iohc->payload.packet.header.source, 3);

            packet->delayed = 250;
            packet->repeat = 0;

            radioInstance->send(packet);
            digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
            break;
        }
        case iohcDevice::RECEIVED_KEY_TRANSFERT_0x32: {
            if (!Cmd::pairMode ||
                twoWPairSession.stage != TwoWPairStage::AwaitKeyTransfer ||
                !isPairingPeer(iohc)) {
                break;
            }
            if (iohc->buffer_length < 25) {
                addLogMessage("2W pair error: short 0x32 key transfer");
                break;
            }

            const uint8_t *encryptedKey = iohc->payload.buffer + 9;
            twoWPairSession.transferredFrameData[0] =
                iohcDevice::RECEIVED_KEY_TRANSFERT_0x32;
            memcpy(twoWPairSession.transferredFrameData.data() + 1,
                   encryptedKey, 16);

            // Captures disagree whether the pull-key IV identifies 0x31 or
            // 0x38. Authenticate both candidates before persisting either.
            twoWPairSession.candidateKeys[0] =
                decryptTransferredKey(encryptedKey, iohcDevice::SEND_ASK_CHALLENGE_0x31);
            twoWPairSession.candidateKeys[1] =
                decryptTransferredKey(encryptedKey, iohcDevice::SEND_LAUNCH_KEY_TRANSFERT_0x38);

            fillRandom(twoWPairSession.authenticationChallenge);
            std::vector<uint8_t> challenge(
                twoWPairSession.authenticationChallenge.begin(),
                twoWPairSession.authenticationChallenge.end());
            auto *packet = makeTwoWPairPacket(
                iohcDevice::SEND_CHALLENGE_REQUEST_0x3C, challenge,
                twoWPairSession.peer, iohc->frequency,
                false, false, false);
            twoWPairSession.frequency = iohc->frequency;
            twoWPairSession.stage = TwoWPairStage::AwaitKeyAuthentication;
            addLogMessage("2W pair step: received 0x32 key; authenticating with 0x3C");
            radioInstance->send(packet);
            break;
        }
        case iohcDevice::RECEIVED_LAUNCH_KEY_TRANSFERT_0x38: {
            printf("2W Key Transfert Asked after Command %2.2X\n", iohc->payload.packet.header.cmd);
            if (!Cmd::pairMode) break;
            addLogMessage("2W pair step: received 0x38 key transfer request; sending 0x32 key transfer");

            std::vector<uint8_t> key_transfert;
            key_transfert.assign(iohc->payload.buffer + 9, iohc->payload.buffer + 15);

            for (unsigned char i: key_transfert) {
                printf("%02X ", i);
            }
            printf("\n");
            std::vector<uint8_t> data = {IOHC::iohcDevice::SEND_ASK_CHALLENGE_0x31}; //0x38
            unsigned char initial_value[16];
            constructInitialValue(data, initial_value, data.size(), key_transfert, nullptr);
            Serial.printf("2) Initial value used for key encryption: ");
            for (unsigned char i: initial_value) {
                printf("%02X ", i);
            }
            printf("\n");

            AES_init_ctx(&ctx, transfert_key);
            uint8_t encrypted_key[16];
            AES_ECB_encrypt(&ctx, initial_value);
            //  XORing transfert_key
            for (int i = 0; i < 16; i++) {
                encrypted_key[i] = initial_value[i] ^ transfert_key[i];
            }
            printf("2) Encrypted 2-way key to be sent with SEND_KEY_TRANSFERT_0x32: ");
            for (unsigned char i: encrypted_key) {
                printf("%02X ", i);
            }
            printf("\n");
            std::vector<uint8_t> toSend;
            toSend.assign(encrypted_key, encrypted_key + 16);

            auto* packet = new iohcPacket;
            forgePacket(packet, toSend);

            packet->payload.packet.header.cmd = IOHC::iohcDevice::SEND_KEY_TRANSFERT_0x32;
            cozyDevice2W->memorizeSend.memorizedCmd = IOHC::iohcDevice::SEND_KEY_TRANSFERT_0x32;

            /* Swap */
            memcpy(packet->payload.packet.header.source, iohc->payload.packet.header.target, 3);
            memcpy(packet->payload.packet.header.target, iohc->payload.packet.header.source, 3);

            packet->repeat = 0;

            radioInstance->send(packet);
            digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
            break;
        }
        case iohcDevice::RECEIVED_WRITE_PRIVATE_0x20:  {
            cozyDevice2W->memorizeSend.memorizedCmd = iohc->payload.packet.header.cmd;
            IOHC::lastSendCmd.store(iohc->payload.packet.header.cmd);
            break;
        }
        case iohcDevice::RECEIVED_PRIVATE_ACK_0x21: {
            // Answer of 0x20, publish the confirmed command
            // doc["type"] = "Cozy";
            // doc["from"] = bytesToHexString(iohc->payload.packet.header.target, 3);
            // doc["to"] = bytesToHexString(iohc->payload.packet.header.source, 3);
            // doc["cmd"] = to_hex_str(iohc->payload.packet.header.cmd).c_str();
            // doc["_data"] = bytesToHexString(iohc->payload.buffer + 9, iohc->buffer_length - 9);
            // std::string message;
            // size_t messageSize = serializeJson(doc, message);
            // mqttClient.publish("iown/Frame", 0, false, message.c_str(), messageSize);
            break;
        }
        case iohcDevice::RECEIVED_CHALLENGE_REQUEST_0x3C: {
            // Answer only to our gateway, not to others devices
            if (cozyDevice2W->isFake(iohc->payload.packet.header.source, iohc->payload.packet.header.target)) {
                // (true) { //

                doc["type"] = "Gateway";
//                if (!cozyDevice2W->isFake(iohc->payload.packet.header.source, iohc->payload.packet.header.target)) {
                    //                        AES_init_ctx(&ctx, setgo); // PreInit AES for other2W (1W use original version) TODO
//                }
                //                    else
                AES_init_ctx(&ctx, transfert_key);

                // IVdata is the challenge with commandId put on start
                std::vector<uint8_t> challengeAsked;
                //                    challengeAsked.assign(iohc->payload.packet.msg.variableData.data, iohc->payload.packet.msg.variableData.data + iohc->payload.packet.msg.variableData.size);
                challengeAsked.assign(iohc->payload.buffer + 9, iohc->payload.buffer + 15);
                const auto lastSendCmd = IOHC::lastSendCmd.load();
                printf("Challenge asked after LastSend Command %2.2X\n", static_cast<unsigned>(lastSendCmd));
                printf("Challenge asked after Memorized Command %2.2X\n", cozyDevice2W->memorizeSend.memorizedCmd);

                if (Cmd::scanMode) {
                    otherDevice2W->mapValid[lastSendCmd] = iohcDevice::RECEIVED_CHALLENGE_REQUEST_0x3C;
                    break;
                }

                std::vector<uint8_t> IVdata = cozyDevice2W->memorizeSend.memorizedData;
                IVdata.insert(IVdata.begin(), cozyDevice2W->memorizeSend.memorizedCmd);

                auto* packet = new iohcPacket;

                packet->payload.packet.header.cmd = IOHC::iohcDevice::SEND_CHALLENGE_ANSWER_0x3D;

                unsigned char initial_value[16];
                constructInitialValue(IVdata, initial_value, IVdata.size(), challengeAsked, nullptr);
                AES_ECB_encrypt(&ctx, initial_value);
                uint8_t dataLen = 6;

                if (cozyDevice2W->memorizeSend.memorizedCmd == IOHC::iohcDevice::RECEIVED_ASK_CHALLENGE_0x31) {
                    packet->payload.packet.header.cmd = IOHC::iohcDevice::SEND_KEY_TRANSFERT_0x32;
                    dataLen = 16;
                    IVdata = {IOHC::iohcDevice::RECEIVED_ASK_CHALLENGE_0x31};
                    constructInitialValue(IVdata, initial_value, 1, challengeAsked, nullptr);
                    AES_ECB_encrypt(&ctx, initial_value);
                    for (int i = 0; i < dataLen; i++)
                        initial_value[i] = initial_value[i] ^ transfert_key[i];
                    cozyDevice2W->memorizeSend.memorizedCmd = IOHC::iohcDevice::SEND_KEY_TRANSFERT_0x32;
                    cozyDevice2W->memorizeSend.memorizedData.assign(initial_value, initial_value + 16);
                }

                std::vector<uint8_t> toSend;
                toSend.assign(initial_value, initial_value + dataLen);
                forgePacket(packet, toSend);

                /* Swap */
                memcpy(packet->payload.packet.header.source, iohc->payload.packet.header.target, 3);
                memcpy(packet->payload.packet.header.target, iohc->payload.packet.header.source, 3);

                packet->repeatTime = 6;
                packet->repeat = 1;

                radioInstance->send(packet);

                // Serial.print("IV used for key encryption: ");
                // for (int i = 0; i < 16; i++)
                //     Serial.printf("%02X ", initial_value[i]);
                // Serial.println();
                printf("Challenge response %2.2X: ", packet->payload.packet.header.cmd);
                for (int i = 0; i < dataLen; i++)
                    printf("%02X ", initial_value[i]);
                printf("\n");

                //                sysTable->addObject(iohc);
                digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
            }
            break;
        }
        case iohcDevice::RECEIVED_CHALLENGE_ANSWER_0x3D: {
            if (!Cmd::pairMode ||
                twoWPairSession.stage != TwoWPairStage::AwaitKeyAuthentication ||
                !isPairingPeer(iohc)) {
                break;
            }
            if (iohc->buffer_length < 15) {
                addLogMessage("2W pair error: short 0x3D authentication answer");
                break;
            }

            const uint8_t *answer = iohc->payload.buffer + 9;
            int authenticatedCandidate = -1;
            for (size_t i = 0; i < twoWPairSession.candidateKeys.size(); ++i) {
                if (authenticateTransferredKey(
                        answer, twoWPairSession.candidateKeys[i])) {
                    authenticatedCandidate = static_cast<int>(i);
                    break;
                }
            }
            if (authenticatedCandidate < 0) {
                addLogMessage("2W pairing failed: transferred key authentication rejected");
                Cmd::pairMode = false;
                Cmd::pairAltMode = false;
                radioInstance->stopTwoWScan();
                resetTwoWPairingSession();
                break;
            }

            persistTwoWSystemKey(
                twoWPairSession.candidateKeys[authenticatedCandidate]);

            uint8_t actuator[2] = {
                twoWPairSession.metadata[0],
                twoWPairSession.metadata[1]
            };
            uint8_t backbone[3] = {
                twoWPairSession.metadata[2],
                twoWPairSession.metadata[3],
                twoWPairSession.metadata[4]
            };
            sysTable->addObject(
                twoWPairSession.peer, backbone, actuator,
                twoWPairSession.metadata[5],
                twoWPairSession.metadata[6]);

            addLogMessage(
                "2W pairing completed with " +
                String(bytesToHexString(twoWPairSession.peer, 3).c_str()));
            Cmd::pairMode = false;
            Cmd::pairAltMode = false;
            radioInstance->stopTwoWScan();
            resetTwoWPairingSession();
            break;
        }
        case 0X00:
        case 0x01:
        case 0x03:
        case 0x19: {
            if (iohc->payload.packet.header.CtrlByte1.asStruct.Protocol == 1 && iohc->payload.packet.header.cmd == 0x00) {
                doc["type"] = "1W";
                uint16_t main = (iohc->payload.packet.msg.p0x00_14.main[0] << 8) | iohc->payload.packet.msg.p0x00_14.main[1];
                const char *action = "unknown";
                switch (main) {
                    case 0x0000: action = "OPEN"; break;
                    case 0xC800: action = "CLOSE"; break;
                    case 0xD200: action = "STOP"; break;
                    case 0xD803: action = "VENT"; break;
                    case 0x6400: action = "FORCE"; break;
                    default: break;
                }
                doc["action"] = action;
                display1WAction(iohc->payload.packet.header.source, action, "RX");
                if (const auto *map = remoteMap->find(iohc->payload.packet.header.source)) {
                    IOHC::RemoteButton btn;
                    if (!strcmp(action, "OPEN")) btn = IOHC::RemoteButton::Open;
                    else if (!strcmp(action, "CLOSE")) btn = IOHC::RemoteButton::Close;
                    else if (!strcmp(action, "STOP")) btn = IOHC::RemoteButton::Stop;
                    else if (!strcmp(action, "VENT")) btn = IOHC::RemoteButton::Vent;
                    else if (!strcmp(action, "FORCE")) btn = IOHC::RemoteButton::ForceOpen;
                    else btn = IOHC::RemoteButton::Stop; // default to avoid uninitialized
                    for (const auto &desc : map->devices) {
                        iohcRemote1W::getInstance()->handleRemoteAction(btn, desc);
                    }
                }
            } else {
                doc["type"] = "Other";
                otherDevice2W->memorizeOther2W.memorizedCmd = iohc->payload.packet.header.cmd;
                cozyDevice2W->memorizeSend.memorizedCmd = iohc->payload.packet.header.cmd;
            }
            break;
        }
        case iohcDevice::RECEIVED_GET_NAME_0x50: {
            if (cozyDevice2W->isFake(iohc->payload.packet.header.source, iohc->payload.packet.header.target)) {
            // MY_GATEWAY 4d595f47415445574159
            std::vector<uint8_t> toSend = {0x4d, 0x59, 0x5f, 0x47, 0x41, 0x54, 0x45, 0x57, 0x41, 0x59};
            toSend.resize(16);

            auto* packet = new iohcPacket;

            forgePacket(packet, toSend);

            packet->payload.packet.header.cmd = 0x51;

            /* Swap */
            memcpy(packet->payload.packet.header.source, cozyDevice2W->gateway, 3);
            memcpy(packet->payload.packet.header.target, iohc->payload.packet.header.source, 3);

            packet->delayed = 50;
            packet->repeat = 0;

            radioInstance->send(packet);
            digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
            }
            break;
        }
        case 0x51: {
            std::vector<uint8_t> nameReceived;
            nameReceived.assign(iohc->payload.buffer + 9, iohc->payload.buffer + 25);
            //            std::string asciiName;

            for (char byte: nameReceived) {
                //    asciiName += std::toupper(byte);
                printf("%c", std::toupper(byte));
            }
            //            printf("%s\n", asciiName.c_str());
            printf("\n");
            break;
        }
        case iohcDevice::RECEIVED_DISCOVER_ACTUATOR_ACK_0x2D: {
            if (Cmd::pairMode &&
                twoWPairSession.stage == TwoWPairStage::AwaitActuatorAck &&
                isPairingPeer(iohc)) {
                fillRandom(twoWPairSession.transferChallenge);
                std::vector<uint8_t> challenge(
                    twoWPairSession.transferChallenge.begin(),
                    twoWPairSession.transferChallenge.end());
                auto *packet = makeTwoWPairPacket(
                    iohcDevice::SEND_LAUNCH_KEY_TRANSFERT_0x38, challenge,
                    twoWPairSession.peer, iohc->frequency,
                    true, false, true);
                twoWPairSession.frequency = iohc->frequency;
                twoWPairSession.stage = TwoWPairStage::AwaitKeyTransfer;
                addLogMessage("2W pair step: received 0x2D ack; requesting key with 0x38");
                radioInstance->send(packet);
                break;
            }
            if (Cmd::scanMode) {
                otherDevice2W->memorizeOther2W = {};
                otherDevice2W->mapValid[IOHC::lastSendCmd.load()] =
                    iohc->payload.packet.header.cmd;
            }
            break;
        }
        case 0x04:
        case 0x0D:
        case 0x4B:
        case 0x55:
        case 0x57:
        case 0x59: {
            if (Cmd::scanMode) {
                otherDevice2W->memorizeOther2W = {};
                // printf(" Answer %X Cmd %X ", iohc->payload.packet.header.cmd, IOHC::lastSendCmd);
                otherDevice2W->mapValid[IOHC::lastSendCmd.load()] = iohc->payload.packet.header.cmd;
            }
        break;
    }
        case iohcDevice::RECEIVED_STATUS_0xFE: {
            if (Cmd::scanMode) {
                otherDevice2W->memorizeOther2W = {};
                // printf(" Unknown %X Cmd %X ", iohc->payload.buffer[9], IOHC::lastSendCmd);
                otherDevice2W->mapValid[IOHC::lastSendCmd.load()] = iohc->payload.buffer[9];
            }
            break;
        }
        case 0x30: {
            for (uint8_t idx = 0; idx < 16; idx++)
                keyCap[idx] = iohc->payload.packet.msg.p0x30.enc_key[idx];

            iohcCrypto::encrypt_1W_key((const uint8_t *) iohc->payload.packet.header.source, (uint8_t *) keyCap);
            printf("CLEAR KEY: ");
            for (unsigned char idx: keyCap)
                printf("%2.2X", idx);
            printf("\n");
            break;
        }
        case 0X2E: {
            printf("1W Learning mode\n");
            if (Cmd::pairMode) {
                addLogMessage("2W pair step: 1W learn frame received from " + deviceId);
                if (Cmd::pairAltMode) {
                    const uint32_t nowMs = millis();
                    if (nowMs - lastTwoWLearnDiscoverMs > 2500UL) {
                        lastTwoWLearnDiscoverMs = nowMs;
                        addLogMessage("2W alt pair step: triggering discover2A after 1W learn frame");
                        otherDevice2W->cmd(IOHC::Other2WButton::discover2A, nullptr);
                    }
                }
            }
            break;
        }
        case 0x39: {
            if (keyCap[0] == 0) break;
            uint8_t hmac[16];
            std::vector<uint8_t> frame(&iohc->payload.packet.header.cmd, &iohc->payload.packet.header.cmd + 2);
            // frame = {0x39, 0x00}; //
            iohcCrypto::create_1W_hmac(hmac, iohc->payload.packet.msg.p0x39.sequence, keyCap, frame);
            printf("MAC: ");
            for (uint8_t idx = 0; idx < 6; idx++)
                printf("%2.2X", hmac[idx]);
            printf("\n");
            break;
        }
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0X05:
        default:
            break;
    }

    publishMsg(iohc);
    return true;
}

/**
 * The function creates a JSON message from an `iohcPacket` object and publishes it using
 * MQTT if enabled.
 *
 * @param iohc The `iohc` parameter is a pointer to an object of type `IOHC::iohcPacket`. The function
 * `publishMsg` takes this pointer as input and processes the data within the `iohc` object to create a
 * JSON message and publish it using MQTT if the conditions are met.
 *
 * @return The function `publishMsg` is returning `false`.
 */
bool publishMsg(IOHC::iohcPacket *iohc) {
    JsonDocument doc;

    doc["type"] = "Cozy";
    doc["from"] = bytesToHexString(iohc->payload.packet.header.target, 3);
    doc["to"] = bytesToHexString(iohc->payload.packet.header.source, 3);
    doc["cmd"] = to_hex_str(iohc->payload.packet.header.cmd).c_str();
    const uint8_t publishDataLen = iohc->buffer_length >= sizeof(_header)
                                       ? iohc->buffer_length - sizeof(_header)
                                       : 0;
    doc["_data"] = publishDataLen
                       ? bytesToHexString(iohc->payload.buffer + sizeof(_header), publishDataLen)
                       : "";
    if (remoteMap) {
        if (const auto *map = remoteMap->find(iohc->payload.packet.header.source)) {
            doc["remote"] = map->name;
        }
    }

    if (iohc->payload.packet.header.CtrlByte1.asStruct.Protocol == 1 &&
        iohc->payload.packet.header.cmd == 0x00) {
        uint16_t main =
                (iohc->payload.packet.msg.p0x00_14.main[0] << 8) |
                iohc->payload.packet.msg.p0x00_14.main[1];
        const char *action = "unknown";
        switch (main) {
            case 0x0000: action = "open"; break;
            case 0xC800: action = "close"; break;
            case 0xD200: action = "stop"; break;
            case 0xD803: action = "vent"; break;
            case 0x6400: action = "force"; break;
            default: break;
        }
        doc["type"] = "1W";
        doc["action"] = action;
    }

    std::string message;
    size_t messageSize = serializeJson(doc, message);
#if defined(MQTT)
    mqttClient.publish("iown/Frame", 1, false, message.c_str(), messageSize);
    mqttClient.publish((mqtt_discovery_topic + "/sensor/iohc_frame/state").c_str(), 0, false, message.c_str(), messageSize);
#endif
    return false;
}

/**
 * @deprecated
 * The function copies data from one `iohcPacket` object to another and stores it in an
 * array, returning true if successful and false if there are not enough buffers available.
 *
 * @param iohc The `iohc` parameter in the `msgArchive` function is a pointer to an object of type
 * `IOHC::iohcPacket`. This object contains information such as buffer length, frequency, RSSI
 * (Received Signal Strength Indication), and payload data. The function `msgArchive` is
 *
 * @return The function `msgArchive` returns a boolean value - `true` if the operation is successful
 * and `false` if there is a failure condition detected during the execution of the function.
 */
bool msgArchive(IOHC::iohcPacket *iohc) {
    if (radioPackets[nextPacket]) {
        delete radioPackets[nextPacket];
        radioPackets[nextPacket] = nullptr;
    }
    radioPackets[nextPacket] = new IOHC::iohcPacket;
    if (!radioPackets[nextPacket]) {
        Serial.printf("*** Malloc failed!\n");
        return false;
    }

    radioPackets[nextPacket]->buffer_length = iohc->buffer_length;
    radioPackets[nextPacket]->frequency = iohc->frequency;
    //    radioPackets[nextPacket]->stamp = iohc->stamp;
    radioPackets[nextPacket]->rssi = iohc->rssi;

    for (uint8_t i = 0; i < iohc->buffer_length; i++)
        radioPackets[nextPacket]->payload.buffer[i] = iohc->payload.buffer[i];

    nextPacket += 1;
    Serial.printf("-> %d\r", nextPacket);
    if (nextPacket >= IOHC_INBOUND_MAX_PACKETS) {
        nextPacket = IOHC_INBOUND_MAX_PACKETS - 1;
        Serial.printf("*** Not enough buffers available. Please erase current ones\n");
        return false;
    }

    return true;
}

/**
 * @deprecated
 * The function `txUserBuffer` sends a packet using a radio instance based on the input command and
 * frequency.
 *
 * @param cmd The `cmd` parameter is a pointer to a `Tokens` object. It seems like the `Tokens` class
 * has a method `size()` that returns the size of the object, and an `at()` method that retrieves a
 * specific element at a given index. The function `txUserBuffer`
 *
 * @return In the provided code snippet, the `txUserBuffer` function returns `void`, which means it
 * does not return any value. Instead, it performs certain operations and then exits the function
 * without returning any specific value.
 */
void txUserBuffer(Tokens *cmd) {
    if (cmd->size() < 2) {
        Serial.printf("No packet to be sent!\n");
        return;
    }
    digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
    auto *packet = new iohcPacket;

    if (cmd->size() == 3)
        packet->frequency = frequencies[atoi(cmd->at(2).c_str()) - 1];
    else
        packet->frequency = 0;

    packet->buffer_length = hexStringToBytes(cmd->at(1), packet->payload.buffer);
    packet->repeatTime = 35;
    packet->repeat = 1;

    radioInstance->send(packet);
    digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
}

void loop() {
    loopWebServer(); // For ESPAsyncWebServer, this is typically not needed.
}
