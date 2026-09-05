/*
 * Modifications Copyright 2026 CloudAXS.
 * Original upstream portions remain licensed under Apache-2.0.
 */
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

#include <iohcCozyDevice2W.h>
#include <LittleFS.h>
#include <iohcCryptoHelpers.h>
#include <ArduinoJson.h>
#include <numeric>

namespace IOHC {
    iohcCozyDevice2W *iohcCozyDevice2W::_iohcCozyDevice2W = nullptr;

    iohcCozyDevice2W::iohcCozyDevice2W() = default;

    iohcCozyDevice2W *iohcCozyDevice2W::getInstance() {
        if (!_iohcCozyDevice2W) {
            _iohcCozyDevice2W = new iohcCozyDevice2W();
            _iohcCozyDevice2W->load();
            _iohcCozyDevice2W->initializeValid();
        }
        return _iohcCozyDevice2W;
    }

    /**
    * @brief Forge a Cozy packet into iohcPacket. This is the function that is called when calling a command
    * @param packet * The IOHC packet to forge
    * @param toSend
    */
    void iohcCozyDevice2W::forgePacket(iohcPacket *packet, const std::vector<unsigned char> &toSend) {
        digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
        IOHC::relStamp.store(esp_timer_get_time());

        // Common Flags
        // 8 if protocol version is 0 else 10
        packet->payload.packet.header.CtrlByte1.asStruct.MsgLen = sizeof(_header) - 1;
        packet->payload.packet.header.CtrlByte1.asStruct.Protocol = 0;
        packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;
        packet->payload.packet.header.CtrlByte1.asStruct.EndFrame = 0;
        packet->payload.packet.header.CtrlByte2.asByte = 0;

        packet->payload.packet.header.CtrlByte1.asByte += toSend.size();
        memcpy(packet->payload.buffer + 9, toSend.data(), toSend.size());
        packet->buffer_length = toSend.size() + 9;

        packet->frequency = CHANNEL2;
        packet->repeatTime = 25;
        packet->repeat = 0;
        packet->lock = false;
    }

    /**
    * @brief Checks if this cozy our fake gateway. This is used to detect if we have an IOCHA device that is in charge of the IOCHA and should be woken up.
    * @param nodeSrc The source node address of the IOCHA.
    * @param nodeDst The destination node address of the IOCHA.
    * @return true if this device is a fake false otherwise. Note that this device is not a fake
    */
    bool iohcCozyDevice2W::isFake(address nodeSrc, address nodeDst) {
        this->Fake = false;
        // Fake to ensure that the node is in the same node src and dst.
        if (!memcmp(this->gateway, nodeSrc, 3) || !memcmp(this->gateway, nodeDst, 3)) { this->Fake = true; }
        return this->Fake;
    }

    /// Emulates device button press
    void iohcCozyDevice2W::cmd(DeviceButton cmd, Tokens *data) {
        if (!_radioInstance) {
            Serial.println("NO RADIO INSTANCE");
            _radioInstance = IOHC::iohcRadio::getInstance();
        }

        switch (cmd) {
            case DeviceButton::associate: {
                std::vector<uint8_t> toSend = {};

                auto* packet = new iohcPacket;
                forgePacket(packet, toSend);

                packet->payload.packet.header.cmd = iohcDevice::SEND_ASK_CHALLENGE_0x31;
                memorizeSend.memorizedData = toSend;
                memorizeSend.memorizedCmd = iohcDevice::SEND_ASK_CHALLENGE_0x31;

                packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;
                packet->payload.packet.header.CtrlByte1.asStruct.EndFrame = 1;

                memcpy(packet->payload.packet.header.source, gateway, 3);
                memcpy(packet->payload.packet.header.target, master_to, 3);

                digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
                _radioInstance->send(packet);
                break;
            }
            case DeviceButton::powerOn: {
                std::vector<uint8_t> toSend = {0x0C, 0x60, 0x01, 0x2C};

                auto* packet = new iohcPacket;
                forgePacket(packet, toSend);

                packet->payload.packet.header.cmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;
                memorizeSend.memorizedCmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;
                memorizeSend.memorizedData = toSend;

                packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;

                memcpy(packet->payload.packet.header.source, gateway, 3);
                memcpy(packet->payload.packet.header.target, master_to, 3);

                digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
                _radioInstance->send(packet);

                break;
            }
            case DeviceButton::setTemp: {
                std::vector<uint8_t> toSend = {0x0C, 0x61, 0x01, 0x03, 0xFF, 0x00};

                int temp = 10 * std::stof(data->at(1));
                toSend[4] = temp;

                int addr = 0;
                if (data->size() == 2) addr = 0;
                else addr = std::stoi(data->at(2));

                auto* packet = new iohcPacket;
                forgePacket(packet, toSend);

                packet->payload.packet.header.cmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;
                memorizeSend.memorizedData = toSend;
                memorizeSend.memorizedCmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;

                packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;

                memcpy(packet->payload.packet.header.source, gateway, 3);
                memcpy(packet->payload.packet.header.target, addresses.at(addr).data()/* 0 Master_to*/, 3);

                packet->delayed = 50;

                digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
                _radioInstance->send(packet);
                //                mqttClient.publish("iown/Frame", 0, false, message.c_str(), messageSize);

                break;
            }
            case DeviceButton::setMode: {
                std::vector<uint8_t> toSend = {0x0C, 0x61, 0x01, 0x00, 0xFF};

                const char *dat = data->at(1).c_str();
                if (strcasecmp(dat, "auto") == 0) toSend[4] = 0x00;
                if (strcasecmp(dat, "manual") == 0) toSend[4] = 0x01;
                if (strcasecmp(dat, "prog") == 0) toSend[4] = 0x02;
                // if (strcasecmp(data, "special") == 0) toSend[4] = 0x03;
                if (strcasecmp(dat, "off") == 0) toSend[4] = 0x04; // TODO if mode off, disable setPresence

                // int addr = 0;
                // if (data->size() == 2) addr = 0;
                // else addr = std::stoi(data->at(2));

                size_t dest = 0;

                std::vector<iohcPacket *> packets2send;
                for (const auto &addr: addresses) {
                    auto* packet = new iohcPacket;
                    packets2send.push_back(packet);
                    forgePacket(packet, toSend);

                    packet->payload.packet.header.cmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;
                    memorizeSend.memorizedData = toSend;
                    memorizeSend.memorizedCmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;

                    packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;
                    memcpy(packet->payload.packet.header.source, gateway, 3);
                    memcpy(packet->payload.packet.header.target,
                           addresses.at(dest/*addr*/).data()/* 0 Master_to*/, 3);

                    dest++;
                }
                packets2send[1]->delayed = 250;
                digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);

                _radioInstance->send(packets2send);

                break;
            }
            case DeviceButton::setPresence: {
                std::vector<uint8_t> toSend = {0x0C, 0x61, 0x01, 0x10, 0xFF};

                const char *dat = data->at(1).c_str();
                if (strcasecmp(dat, "on") == 0) toSend[4] = 0x01;
                if (strcasecmp(dat, "off") == 0) toSend[4] = 0x00;

                auto* packet = new iohcPacket;
                forgePacket(packet, toSend);

                packet->payload.packet.header.cmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;
                memorizeSend.memorizedData = toSend;
                memorizeSend.memorizedCmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;

                packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;

                memcpy(packet->payload.packet.header.source, gateway, 3);
                memcpy(packet->payload.packet.header.target, master_to, 3);

                digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
                _radioInstance->send(packet);
                break;
            }
            case DeviceButton::setWindow: {
                std::vector<uint8_t> toSend = {0x0C, 0x61, 0x01, 0x0E, 0xFF};

                const char *dat = data->at(1).c_str();
                if (strcasecmp(dat, "open") == 0) toSend[4] = 0x01;
                if (strcasecmp(dat, "close") == 0) toSend[4] = 0x00;

                int addr = 0;
                if (data->size() == 2) addr = 0;
                else addr = std::stoi(data->at(2));

                auto* packet = new iohcPacket;
                forgePacket(packet, toSend);

                packet->payload.packet.header.cmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;
                memorizeSend.memorizedData = toSend;
                memorizeSend.memorizedCmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;

                packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;

                memcpy(packet->payload.packet.header.source, gateway, 3);
                memcpy(packet->payload.packet.header.target, addresses.at(addr).data()/* 0 Master_to*/, 3);

                packet->delayed = 50;

                digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
                _radioInstance->send(packet);
                break;
            }
            case DeviceButton::midnight: {
                // std::vector<uint8_t> toSend = {0x00, 0x0c, 0x00, 0x00, 0x03, 0x00, 0x00, 0x01, 0x53};
                std::vector<uint8_t> toSend = {0x0c, 0x60, 0x01, 0x30};
                //, 0x2b, 0x05, 0x00, 0x0f, 0x04, 0x0c, 0xe7, 0x07};

                auto *packet = new iohcPacket;
                forgePacket(packet, toSend);

                packet->payload.packet.header.cmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;
                memorizeSend.memorizedData = toSend;
                memorizeSend.memorizedCmd = iohcDevice::SEND_WRITE_PRIVATE_0x20;

                packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;

                memcpy(packet->payload.packet.header.source, gateway, 3);
                memcpy(packet->payload.packet.header.target, master_to, 3);

                digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
                _radioInstance->send(packet);

                break;
            }

            default: break;
        } // switch (cmd)
        IOHC::packetStamp.store(esp_timer_get_time());
        //        save(); // Save Cozy associated devices
    }

    /**
    * @brief Load Cozy 2W settings from file and store in _radioInstance.
    * @return True if successful false otherwise. This is a blocking call
    */
    bool iohcCozyDevice2W::load() {
        _radioInstance = iohcRadio::getInstance();
        // Load Cozy 2W device settings from file
        if (LittleFS.exists(COZY_2W_FILE))
            Serial.printf("Loading Cozy 2W devices settings from %s\n", COZY_2W_FILE);
        else {
            Serial.printf("*2W Cozy devices not available\n");
            return false;
        }

        fs::File f = LittleFS.open(COZY_2W_FILE, "r", true);
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, f);
        f.close();

        if (error) {
            Serial.print("Failed to parse JSON: ");
            Serial.println(error.c_str());
            return false;
        }


        // Iterate through the JSON object
        for (JsonPair kv: doc.as<JsonObject>()) {
            const std::string key = kv.key().c_str();
            if (key.rfind("_", 0) == 0 || !kv.value().is<JsonObject>() || key.length() != sizeof(address) * 2) {
                continue;
            }
            device d;
            if (hexStringToBytes(key, d._node) != sizeof(d._node)) {
                continue;
            }
            auto jobj = kv.value().as<JsonObject>();
            if (jobj["type"].is<const char*>()) d._type = jobj["type"].as<std::string>();
            if (jobj["description"].is<const char*>()) d._description = jobj["description"].as<std::string>();
            //     hexStringToBytes(jobj["key"].as<const char*>(), _key);
            if (jobj["dst"].is<const char*>()) hexStringToBytes(jobj["dst"].as<const char *>(), d._dst);
            //     uint8_t btmp[2];
            //     hexStringToBytes(jobj["sequence"].as<const char*>(), btmp);
            //            /*hexStringToBytes*/(jobj["type"].as<const char*>(), _type);
            //     _sequence = (btmp[0]<<8)+btmp[1];
            //     JsonArray jarr = jobj["type"];
            //     // Iterate through the JSON array
            //     for (uint8_t i=0; i<jarr.size(); i++)
            //         _type.insert(_type.begin()+i, jarr[i].as<uint16_t>());
            //     _manufacturer = jobj["manufacturer_id"].as<uint8_t>();
            devices.push_back(d);
        }
        Serial.printf("Loaded %d x 2W devices\n", devices.size()); // _type.size());

        return true;
    }

    /**
     * @brief
     *
     * @return true
     * @return false
     */
    bool iohcCozyDevice2W::save() {
        fs::File f = LittleFS.open(COZY_2W_FILE, "w");

        //DynamicJsonDocument doc(1024);
        JsonDocument doc;

        for (const auto &d: devices) {
            std::string key = bytesToHexString(d._node, sizeof(d._node));
            JsonObject jobj = doc[key.c_str()].to<JsonObject>();
            jobj["dst"] = bytesToHexString(d._dst, sizeof d._dst);
            jobj["type"] = d._type;
            jobj["description"] = d._description;

            //        uint8_t btmp[2];
            //        btmp[1] = _sequence & 0x00ff;
            //        btmp[0] = _sequence >> 8;
            //        jobj["sequence"] = bytesToHexString(btmp, sizeof(btmp));

            //        jobj["_type"] = _type;

            //        JsonArray jarr = jobj.createNestedArray("type");
            //        for (uint8_t i=0; i<_type.size(); i++)
            //            if (_type[i])
            //                jarr.add(_type.at(i));
            //            else
            //                break;
            //        jobj["manufacturer_id"] = _manufacturer;
        }
        serializeJsonPretty/*serializeJson*/(doc, f);
        f.close();

        return true;
    }

    void iohcCozyDevice2W::initializeValid() {
        size_t validKey = 0;
        auto valid = std::vector<uint8_t>(255);
        std::iota(valid.begin(), valid.end(), 0);

        valid = {
            0x00, 0x01, 0x03, 0x0a, 0x0c, 0x19, 0x1e, 0x20, 0x23, 0x28, 0x2a, 0x2c, 0x2e, 0x31, 0x32, 0x36, 0x38, 0x39,
            0x3c, 0x46, 0x48, 0x4a, 0x4b,
            0x50, 0x52, 0x54, 0x56, 0x60, 0x64, 0x6e, 0x6f, 0x71, 0x73, 0x80, 0x82, 0x84, 0x86, 0x88, 0x8a, 0x8b, 0x8e,
            0x90, 0x92, 0x94, 0x96, 0x98,
            //Not in firmware 02 0e 25 30 34 3a 3d 58
            0x02, 0x0b, 0x0e, 0x14, 0x16, 0x25, 0x30, 0x34, 0x3a, 0x3d, 0x58
        };

        for (int key: valid) {
            mapValid[key] = 0;
        }
    }
}
