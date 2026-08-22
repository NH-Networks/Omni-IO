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

#include <iohcRemote1W.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include <iohcCryptoHelpers.h>
#include <esp_system.h>
#include <oled_display.h>
#include <TickerUsESP32.h>
#include <nvs_helpers.h>
#include <cmath>
#include <algorithm>
#if defined(MQTT)
#include <mqtt_handler.h>
#endif
#if defined(WEBSERVER)
#include <web_server_handler.h>
#endif

namespace IOHC {
    iohcRemote1W* iohcRemote1W::_iohcRemote1W = nullptr;
    static constexpr uint32_t DEFAULT_TRAVEL_TIME_SEC = 10;

    static void broadcastWebDeviceAction(const iohcRemote1W::remote &r, const char *action) {
#if defined(WEBSERVER)
        const std::string id = bytesToHexString(r.node, sizeof(r.node));
        broadcastDeviceAction(id.c_str(), action,
                              static_cast<int>(std::round(r.positionTracker.getPosition())),
                              static_cast<int>(std::round(r.targetPosition)), "gateway");
#endif
    }
    static void positionTaskLoop(void *arg) {
        auto *inst = static_cast<iohcRemote1W *>(arg);
        while (true) {
            inst->updatePositions();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    static const char *remoteButtonToString(RemoteButton cmd) {
        switch (cmd) {
            case RemoteButton::Open: return "OPEN";
            case RemoteButton::Close: return "CLOSE";
            case RemoteButton::Stop: return "STOP";
            case RemoteButton::Vent: return "VENT";
            case RemoteButton::ForceOpen: return "FORCE";
            case RemoteButton::Position: return "POSITION";
            case RemoteButton::Absolute: return "ABSOLUTE";
            case RemoteButton::Pair: return "PAIR";
            case RemoteButton::Add: return "ADD";
            case RemoteButton::Remove: return "REMOVE";
            case RemoteButton::Mode1: return "MODE1";
            case RemoteButton::Mode2: return "MODE2";
            case RemoteButton::Mode3: return "MODE3";
            case RemoteButton::Mode4: return "MODE4";
            default: return "UNKNOWN";
        }
    }

    iohcRemote1W::iohcRemote1W() = default;

    iohcRemote1W* iohcRemote1W::getInstance() {
        if (!_iohcRemote1W) {
            _iohcRemote1W = new iohcRemote1W();
            _iohcRemote1W->load();
            xTaskCreatePinnedToCore(positionTaskLoop, "positionTracker", 4096,
                                    _iohcRemote1W, 1, nullptr, 1);
        }
        return _iohcRemote1W;
    }

    void iohcRemote1W::forgePacket(iohcPacket* packet, uint16_t typn) {
        IOHC::relStamp.store(esp_timer_get_time());
        digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);

        packet->payload.packet.header.CtrlByte1.asStruct.MsgLen = sizeof(_header) - 1;
        packet->payload.packet.header.CtrlByte1.asStruct.Protocol = 1;
        packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;
        packet->payload.packet.header.CtrlByte1.asStruct.EndFrame = 1;
        packet->payload.packet.header.CtrlByte2.asByte = 0;
        packet->payload.packet.header.CtrlByte2.asStruct.LPM = 1;
        // Broadcast Target
        uint16_t bcast = (typn << 6) + 0b111111;
        packet->payload.packet.header.target[0] = 0x00;
        packet->payload.packet.header.target[1] = bcast >> 8;
        packet->payload.packet.header.target[2] = bcast & 0x00ff;

        packet->frequency = CHANNEL2;
        packet->repeatTime = 40; //40ms
        packet->repeat = 4;
        packet->lock = false;
    }

    // Forge a 0x30 Add packet for a given remote and add it to packets2send.
    // Uses a fully open broadcast target (0x003F) so the screen in pair mode
    // receives it regardless of its type byte.
    static void forge0x30Packet(iohcRemote1W::remote &r,
                                std::vector<iohcPacket *> &packets2send) {
        auto *packet = new iohcPacket;
        packets2send.push_back(packet);

        // Header defaults
        packet->payload.packet.header.CtrlByte1.asStruct.MsgLen = sizeof(_header) - 1;
        packet->payload.packet.header.CtrlByte1.asStruct.Protocol = 1;
        packet->payload.packet.header.CtrlByte1.asStruct.StartFrame = 1;
        packet->payload.packet.header.CtrlByte1.asStruct.EndFrame = 1;
        packet->payload.packet.header.CtrlByte2.asByte = 0;
        packet->payload.packet.header.CtrlByte2.asStruct.LPM = 1;

        // Fully-open broadcast target: 0x003F  (type=0 -> any type accepted)
        packet->payload.packet.header.target[0] = 0x00;
        packet->payload.packet.header.target[1] = 0x00;
        packet->payload.packet.header.target[2] = 0x3F;

        packet->frequency = CHANNEL2;
        packet->repeatTime = 40;
        packet->repeat = 4;
        packet->lock = false;

        // Packet length
        packet->payload.packet.header.CtrlByte1.asStruct.MsgLen += sizeof(_p0x30);

        // Source (our virtual remote address)
        for (size_t i = 0; i < sizeof(address); i++)
            packet->payload.packet.header.source[i] = r.node[i];

        // Command 0x30 = Add / key exchange
        packet->payload.packet.header.cmd = 0x30;

        // Encrypted key
        uint8_t encKey[16];
        memcpy(encKey, r.key, 16);
        iohcCrypto::encrypt_1W_key(r.node, encKey);
        memcpy(packet->payload.packet.msg.p0x30.enc_key, encKey, 16);

        // Manufacturer
        packet->payload.packet.msg.p0x30.man_id = r.manufacturer;
        // data byte 0x01 = "I am a controller presenting my key"
        packet->payload.packet.msg.p0x30.data = 0x01;
        // Sequence
        packet->payload.packet.msg.p0x30.sequence[0] = r.sequence >> 8;
        packet->payload.packet.msg.p0x30.sequence[1] = r.sequence & 0x00ff;
        r.sequence += 1;
        nvs_write_sequence(r.node, r.sequence);

        packet->buffer_length = packet->payload.packet.header.CtrlByte1.asStruct.MsgLen + 1;
        digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
    }

    std::vector<uint8_t> frame;

    void iohcRemote1W::cmd(RemoteButton cmd, Tokens* data) {
        if (data->size() == 1) {return; }
        std::string description = data->at(1).c_str();

        auto it = std::find_if( remotes.begin(), remotes.end(),  [&] ( const remote &r  ) {
                 return description == r.description;
              } );

        if (it == remotes.end()) {
            printf("ERROR %s NOT IN JSON", description.c_str());
            return;
        }

        remote& r = *it;
        r.positionTracker.update();

        // Emulates remote button press
        switch (cmd) {
            case RemoteButton::Pair: {
                // 0x2e: 0x1120 + target broadcast + source + 0x2e00 + sequence + hmac

                std::vector<iohcPacket *> packets2send;

                    auto* packet = new iohcPacket;
                    packets2send.push_back(packet);
                    IOHC::iohcRemote1W::forgePacket(packet, r.type[0]);
                    // Packet length
                    packet->payload.packet.header.CtrlByte1.asStruct.MsgLen += sizeof(_p0x2e);

                    // Source (me)
                    for (size_t i = 0; i < sizeof(address); i++)
                        packet->payload.packet.header.source[i] = r.node[i];

                    //Command
                    packet->payload.packet.header.cmd = 0x2e;
                    // Data
                    packet->payload.packet.msg.p0x2e.data = 0x00;
                    // Sequence
                    packet->payload.packet.msg.p0x2e.sequence[0] = r.sequence >> 8;
                    packet->payload.packet.msg.p0x2e.sequence[1] = r.sequence & 0x00ff;
                    r.sequence += 1;
                    nvs_write_sequence(r.node, r.sequence);
                    // hmac
                    frame = std::vector(&packet->payload.packet.header.cmd, &packet->payload.packet.header.cmd + 2);
                    uint8_t hmac[16];
                    iohcCrypto::create_1W_hmac(hmac, packet->payload.packet.msg.p0x2e.sequence, r.key, frame);

                    for (uint8_t i = 0; i < 6; i++)
                        packet->payload.packet.msg.p0x2e.hmac[i] = hmac[i];

                    packet->buffer_length = packet->payload.packet.header.CtrlByte1.asStruct.MsgLen + 1;

                    digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);

                _radioInstance->send(packets2send);
                display1WAction(r.node, remoteButtonToString(cmd), "TX", r.name.c_str());

                Serial.printf("%s position: %.0f%%\n", r.name.c_str(), r.positionTracker.getPosition());
                display1WPosition(r.node, r.positionTracker.getPosition(), r.name.c_str());

                r.paired = true;
                break;
            }

            case RemoteButton::Remove: {
                // 0x39: 0x1c00 + target broadcast + source + 0x3900 + sequence + hmac

                std::vector<iohcPacket *> packets2send;

                    auto* packet = new iohcPacket;
                    packets2send.push_back(packet);
                    IOHC::iohcRemote1W::forgePacket(packet, r.type[0]);
                    packet->payload.packet.header.CtrlByte1.asStruct.MsgLen += sizeof(_p0x2e);

                    // Source (me)
                    for (size_t i = 0; i < sizeof(address); i++)
                        packet->payload.packet.header.source[i] = r.node[i];

                    //Command
                    packet->payload.packet.header.cmd = 0x39;
                    // Data
                    packet->payload.packet.msg.p0x2e.data = 0x00;
                    // Sequence
                    packet->payload.packet.msg.p0x2e.sequence[0] = r.sequence >> 8;
                    packet->payload.packet.msg.p0x2e.sequence[1] = r.sequence & 0x00ff;
                    r.sequence += 1;
                    nvs_write_sequence(r.node, r.sequence);
                    // hmac
                    uint8_t hmac2[16];
                    frame = std::vector(&packet->payload.packet.header.cmd, &packet->payload.packet.header.cmd + 2);
                    iohcCrypto::create_1W_hmac(hmac2, packet->payload.packet.msg.p0x2e.sequence, r.key, frame);
                    for (uint8_t i = 0; i < 6; i++)
                        packet->payload.packet.msg.p0x2e.hmac[i] = hmac2[i];

                    packet->buffer_length = packet->payload.packet.header.CtrlByte1.asStruct.MsgLen + 1;

                    digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);

                _radioInstance->send(packets2send);
                display1WAction(r.node, remoteButtonToString(cmd), "TX", r.name.c_str());

                Serial.printf("%s position: %.0f%%\n", r.name.c_str(), r.positionTracker.getPosition());
                display1WPosition(r.node, r.positionTracker.getPosition(), r.name.c_str());

                r.paired = false;
                break;
            }

            case RemoteButton::Add: {
                // 0x30: key exchange broadcast so screen in pair mode stores our key.
                // Uses fully-open broadcast target (0x003F) via forge0x30Packet().
                std::vector<iohcPacket *> packets2send;
                Serial.printf("1W ADD: %s (node %s) -> broadcasting 0x30\n",
                              r.description.c_str(),
                              bytesToHexString(r.node, sizeof(r.node)).c_str());
                forge0x30Packet(r, packets2send);
                _radioInstance->send(packets2send);
                display1WAction(r.node, remoteButtonToString(cmd), "TX", r.name.c_str());
                Serial.printf("%s position: %.0f%%\n", r.name.c_str(), r.positionTracker.getPosition());
                display1WPosition(r.node, r.positionTracker.getPosition(), r.name.c_str());
                r.paired = true;
                break;
            }
           default: {
                // 0x00: 0x1600 + target broadcast + source + 0x00 + Originator + ACEI + Main Param + FP1 + FP2 + sequence + hmac

                std::vector<iohcPacket *> packets2send;

                    auto* packet = new iohcPacket;
                    packets2send.push_back(packet);

                    IOHC::iohcRemote1W::forgePacket(packet, r.type[0]);
                    // Source (me)
                    for (size_t i = 0; i < sizeof(address); i++)
                        packet->payload.packet.header.source[i] = r.node[i];
                    //Command
                    packet->payload.packet.header.cmd = 0x00;
                    packet->payload.packet.msg.p0x00_14.origin = 0x01; // Command Source Originator is: 0x01 User
                    setAcei(packet->payload.packet.msg.p0x00_14.acei, 0x43);
                    switch (cmd) {
                        // Switch for Main Parameter of cmd 0x00: Open/Close/Stop/Ventilation
                        case RemoteButton::Open:
                            packet->payload.packet.msg.p0x00_14.main[0] = 0x00;
                            packet->payload.packet.msg.p0x00_14.main[1] = 0x00;
                            r.positionTracker.startOpening();
                            r.movement = remote::Movement::Opening;
                            r.targetPosition = 100.0f;
                            broadcastWebDeviceAction(r, "OPENING");
#if defined(MQTT)
                            {
                                std::string id = bytesToHexString(r.node, sizeof(r.node));
                                publishCoverState(id, "OPENING");
                                publishCoverPosition(id, r.positionTracker.getPosition());
                                r.lastPublishedState = "OPENING";
                                r.lastPublishedPosition = r.positionTracker.getPosition();
                            }
#endif
                            break;
                        case RemoteButton::Close:
                            packet->payload.packet.msg.p0x00_14.main[0] = 0xc8;
                            packet->payload.packet.msg.p0x00_14.main[1] = 0x00;
                            r.positionTracker.startClosing();
                            r.movement = remote::Movement::Closing;
                            r.targetPosition = 0.0f;
                            broadcastWebDeviceAction(r, "CLOSING");
#if defined(MQTT)
                            {
                                std::string id = bytesToHexString(r.node, sizeof(r.node));
                                publishCoverState(id, "CLOSING");
                                publishCoverPosition(id, r.positionTracker.getPosition());
                                r.lastPublishedState = "CLOSING";
                                r.lastPublishedPosition = r.positionTracker.getPosition();
                            }
#endif
                            break;
                        case RemoteButton::Stop:
                            packet->payload.packet.msg.p0x00_14.main[0] = 0xd2;
                            packet->payload.packet.msg.p0x00_14.main[1] = 0x00;
                            r.positionTracker.stop();
                            r.movement = remote::Movement::Idle;
                            r.targetPosition = r.positionTracker.getPosition();
                            broadcastWebDeviceAction(r, "STOP");
#if defined(MQTT)
                            {
                                std::string id = bytesToHexString(r.node, sizeof(r.node));
                                publishCoverState(id, "STOP");
                                publishCoverPosition(id, r.positionTracker.getPosition());
                                r.lastPublishedState = "STOP";
                                r.lastPublishedPosition = r.positionTracker.getPosition();
                            }
#endif
                            break;
                        case RemoteButton::Vent:
                            packet->payload.packet.msg.p0x00_14.main[0] = 0xd8;
                            packet->payload.packet.msg.p0x00_14.main[1] = 0x03;
                            break;
                        case RemoteButton::ForceOpen:
                            packet->payload.packet.msg.p0x00_14.main[0] = 0x64;
                            packet->payload.packet.msg.p0x00_14.main[1] = 0x00;
                            break;
                        case RemoteButton::Position: {
                            int index = (data->size() > 2) ? 2 : 0;
                            int percent = atoi(data->at(index).c_str());
                            percent = std::clamp(percent, 0, 100);
                            uint8_t val = static_cast<uint8_t>((100 - percent) * 2);
                            packet->payload.packet.msg.p0x00_14.main[0] = val;
                            packet->payload.packet.msg.p0x00_14.main[1] = 0x00;
                            float current = r.positionTracker.getPosition();
                            if (percent > current + 0.5f) {
                                r.positionTracker.startOpening();
                                r.movement = remote::Movement::Opening;
                                #if defined(MQTT)
                                {
                                    std::string id = bytesToHexString(r.node, sizeof(r.node));
                                    publishCoverState(id, "OPENING");
                                    publishCoverPosition(id, r.positionTracker.getPosition());
                                    r.lastPublishedState = "OPENING";
                                    r.lastPublishedPosition = r.positionTracker.getPosition();
                                }
                                #endif
                            } else if (percent < current - 0.5f) {
                                r.positionTracker.startClosing();
                                r.movement = remote::Movement::Closing;
                                #if defined(MQTT)
                                {
                                    std::string id = bytesToHexString(r.node, sizeof(r.node));
                                    publishCoverState(id, "CLOSING");
                                    publishCoverPosition(id, r.positionTracker.getPosition());
                                    r.lastPublishedState = "CLOSING";
                                    r.lastPublishedPosition = r.positionTracker.getPosition();
                                }
                                #endif
                            } else {
                                r.positionTracker.stop();
                                r.movement = remote::Movement::Idle;
                            }
                            r.targetPosition = percent;
                            broadcastWebDeviceAction(r, remoteButtonToString(cmd));
                            break;
                        }
                        case RemoteButton::Absolute: {
                            int index = (data->size() > 2) ? 2 : 0;
                            int percent = atoi(data->at(index).c_str());
                            percent = std::clamp(percent, 0, 100);
                            uint16_t val = static_cast<uint16_t>(percent * 0x0200);
                            packet->payload.packet.msg.p0x00_14.main[0] = val >> 8;
                            packet->payload.packet.msg.p0x00_14.main[1] = val & 0xFF;
                            float target = 100.0f - percent;
                            float current = r.positionTracker.getPosition();
                            if (target > current + 0.5f) {
                                r.positionTracker.startOpening();
                                r.movement = remote::Movement::Opening;
#if defined(MQTT)
                                {
                                    std::string id = bytesToHexString(r.node, sizeof(r.node));
                                    publishCoverState(id, "OPENING");
                                    publishCoverPosition(id, r.positionTracker.getPosition());
                                    r.lastPublishedState = "OPENING";
                                    r.lastPublishedPosition = r.positionTracker.getPosition();
                                }
#endif
                            } else if (target < current - 0.5f) {
                                r.positionTracker.startClosing();
                                r.movement = remote::Movement::Closing;
#if defined(MQTT)
                                {
                                    std::string id = bytesToHexString(r.node, sizeof(r.node));
                                    publishCoverState(id, "CLOSING");
                                    publishCoverPosition(id, r.positionTracker.getPosition());
                                    r.lastPublishedState = "CLOSING";
                                    r.lastPublishedPosition = r.positionTracker.getPosition();
                                }
#endif
                            } else {
                                r.positionTracker.stop();
                                r.movement = remote::Movement::Idle;
                            }
                            r.targetPosition = target;
                            broadcastWebDeviceAction(r, remoteButtonToString(cmd));
                            break;
                        }
                        case RemoteButton::Mode1:{
                            packet->payload.packet.header.cmd = 0x01;
                            packet->payload.packet.msg.p0x01_13.main = 0x00;
                            packet->payload.packet.msg.p0x01_13.fp1 = 0x01;
                            packet->payload.packet.msg.p0x01_13.fp2 = r.sequence & 0xFF;
                            break;
                        }

                        case RemoteButton::Mode2: {
                            packet->payload.packet.header.cmd = 0x01;
                            packet->payload.packet.msg.p0x01_13.main = 0x00;
                            packet->payload.packet.msg.p0x01_13.fp1 = 0x02;
                            packet->payload.packet.msg.p0x01_13.fp2 =  r.sequence & 0xFF;
                            break;
                    }
                    case RemoteButton::Mode3:{
                        break;
                    }
                    case RemoteButton::Mode4: {
                            packet->payload.packet.header.cmd = 0x00;
                            packet->payload.packet.msg.p0x00_16.main[0] = 0xd2;
                            packet->payload.packet.msg.p0x00_16.main[1] = 0x00;
                            packet->payload.packet.msg.p0x00_16.fp1 = 0x20;
                            packet->payload.packet.msg.p0x00_16.fp2 = 0xCD;
                            packet->payload.packet.msg.p0x00_16.data[0] = 0x2E;
                            packet->payload.packet.msg.p0x00_16.data[1] = 0x00;
                             if (packet->payload.packet.header.source[2] == 0x1B) {
                                packet->payload.packet.msg.p0x00_16.fp2 = 0xCC;
                                packet->payload.packet.msg.p0x00_16.data[0] = 0xA2;
                            }
                        break;
                    }
                        default:
                            return;
                    }

                    uint8_t hmac[16];

                    if (r.type[0] == 0 && (cmd == RemoteButton::Mode1 ||  cmd == RemoteButton::Mode2)) {
                        packet->payload.packet.header.CtrlByte1.asStruct.MsgLen += sizeof(_p0x01_13) ;
                        packet->payload.packet.msg.p0x01_13.sequence[0] = r.sequence >> 8;
                        packet->payload.packet.msg.p0x01_13.sequence[1] = r.sequence & 0x00ff;
                        uint8_t toAdd = 5 + 1;
                        frame = std::vector(&packet->payload.packet.header.cmd, &packet->payload.packet.header.cmd + toAdd);
                        iohcCrypto::create_1W_hmac(hmac, packet->payload.packet.msg.p0x01_13.sequence, r.key, frame);
                        for (uint8_t i = 0; i < 6; i++) {
                            packet->payload.packet.msg.p0x01_13.hmac[i] = hmac[i];
                        }
                    }
                    else if (r.type[0] == 0 && (cmd == RemoteButton::Mode4)) {
                        packet->payload.packet.header.CtrlByte1.asStruct.MsgLen += sizeof(_p0x00_16) ;
                        packet->payload.packet.msg.p0x00_16.sequence[0] = r.sequence >> 8;
                        packet->payload.packet.msg.p0x00_16.sequence[1] = r.sequence & 0x00ff;
                        uint8_t toAdd = 8 + 1;
                        frame = std::vector(&packet->payload.packet.header.cmd, &packet->payload.packet.header.cmd + toAdd);
                        iohcCrypto::create_1W_hmac(hmac, packet->payload.packet.msg.p0x00_16.sequence, r.key, frame);
                        for (uint8_t i = 0; i < 6; i++) {
                            packet->payload.packet.msg.p0x00_16.hmac[i] = hmac[i];
                        }
                    }
                    else {
                        packet->payload.packet.header.CtrlByte1.asStruct.MsgLen += sizeof(_p0x00_14) ;
                        packet->payload.packet.msg.p0x00_14.sequence[0] = r.sequence >> 8;
                        packet->payload.packet.msg.p0x00_14.sequence[1] = r.sequence & 0x00ff;
                        uint8_t toAdd = 6 + 1;
                        frame = std::vector(&packet->payload.packet.header.cmd, &packet->payload.packet.header.cmd + toAdd);
                        iohcCrypto::create_1W_hmac(hmac, packet->payload.packet.msg.p0x00_14.sequence, r.key, frame);
                        for (uint8_t i = 0; i < 6; i++) {
                            packet->payload.packet.msg.p0x00_14.hmac[i] = hmac[i];
                        }
                    }

                    r.sequence += 1;
                    nvs_write_sequence(r.node, r.sequence);

                    packet->buffer_length = packet->payload.packet.header.CtrlByte1.asStruct.MsgLen + 1;

                    digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);

                    _radioInstance->send(packets2send);

                    display1WAction(r.node, remoteButtonToString(cmd), "TX", r.name.c_str());
                    Serial.printf("%s position: %.0f%%\n", r.name.c_str(), r.positionTracker.getPosition());
                    display1WPosition(r.node, r.positionTracker.getPosition(), r.name.c_str());
                    break;
                }
        }
        this->save();
    }

    // addBroadcast: send cmd 0x30 for every unpaired (or all) remotes.
    // Called automatically by the discover1W command so the screen receives
    // the key-exchange packet immediately when it enters pair mode.
    // Pass force=true to also re-send for already-paired remotes (e.g. re-pair).
    void iohcRemote1W::addBroadcast(bool force) {
        int count = 0;
        for (auto &r : remotes) {
            if (!force && r.paired) continue;
            std::vector<iohcPacket *> packets2send;
            Serial.printf("1W DISCOVER: advertising %s (node %s)\n",
                          r.description.c_str(),
                          bytesToHexString(r.node, sizeof(r.node)).c_str());
            forge0x30Packet(r, packets2send);
            _radioInstance->send(packets2send);
            display1WAction(r.node, "ADD", "TX", r.name.c_str());
            r.paired = true;
            count++;
            // Small gap between packets so the radio can switch back to RX
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        if (count == 0) {
            Serial.println("1W DISCOVER: no unpaired remotes to advertise. Use 'force' or add a new remote first with new1W.");
        } else {
            save();
        }
    }

   bool iohcRemote1W::load() {
        _radioInstance = iohcRadio::getInstance();

        if (LittleFS.exists(IOHC_1W_REMOTE))
            Serial.printf("Loading 1W remote settings from %s\n", IOHC_1W_REMOTE);
        else {
            Serial.printf("*1W remote not available\n");
            return false;
        }

        fs::File f = LittleFS.open(IOHC_1W_REMOTE, "r");
        JsonDocument doc;

        DeserializationError error = deserializeJson(doc, f);

        if (error) {
            Serial.print("Failed to parse JSON: ");
            Serial.println(error.c_str());
            f.close();
            return false;
        }
        f.close();

        bool updateFile = false;
        std::vector<remote> loadedRemotes;
        for (JsonPair kv: doc.as<JsonObject>()) {
            remote r;
            auto jobj = kv.value().as<JsonObject>();
            const std::string entryId = kv.key().c_str();

            if (hexStringToBytes(entryId, r.node) != sizeof(r.node)) {
                Serial.printf("Skipping 1W remote '%s': invalid id\n", entryId.c_str());
                continue;
            }

            const char *keyText = nullptr;
            if (jobj["key"].is<const char *>()) {
                keyText = jobj["key"].as<const char *>();
            }
            if (!keyText || hexStringToBytes(keyText, r.key) != sizeof(r.key)) {
                Serial.printf("Skipping 1W remote '%s': invalid key\n", entryId.c_str());
                continue;
            }

            const char *sequenceText = nullptr;
            if (jobj["sequence"].is<const char *>()) {
                sequenceText = jobj["sequence"].as<const char *>();
            }
            uint8_t btmp[2] = {0, 0};
            if (!sequenceText || hexStringToBytes(sequenceText, btmp) != sizeof(btmp)) {
                Serial.printf("Skipping 1W remote '%s': invalid sequence\n", entryId.c_str());
                continue;
            }
            r.sequence = static_cast<uint16_t>((btmp[0] << 8) + btmp[1]);

            uint16_t nvs_seq;
            if (nvs_read_sequence(r.node, &nvs_seq)) {
                if (nvs_seq > r.sequence) {
                    r.sequence = nvs_seq;
                    updateFile = true;
                }
            }
            nvs_write_sequence(r.node, r.sequence);
            JsonArray jarr = jobj["type"].as<JsonArray>();
            if (jarr.isNull()) {
                Serial.printf("Skipping 1W remote '%s': missing type array\n", entryId.c_str());
                continue;
            }
            r.type.reserve(jarr.size());
            for (auto && i : jarr) {
                r.type.push_back(i.as<uint8_t>());
            }

            r.manufacturer = jobj["manufacturer_id"].as<uint8_t>();
            if (jobj["description"].is<const char *>()) {
                r.description = jobj["description"].as<const char *>();
            } else if (jobj["description"].is<std::string>()) {
                r.description = jobj["description"].as<std::string>();
            } else {
                r.description = entryId;
                updateFile = true;
            }

            if (jobj["name"].is<const char *>()) {
                r.name = jobj["name"].as<const char *>();
            } else if (jobj["name"].is<std::string>()) {
                r.name = jobj["name"].as<std::string>();
            } else {
                r.name = r.description;
                updateFile = true;
            }

            if (jobj["travel_time"].is<uint32_t>()) {
                r.travelTime = jobj["travel_time"].as<uint32_t>();
            } else {
                r.travelTime = DEFAULT_TRAVEL_TIME_SEC;
                updateFile = true;
            }
            if (jobj["paired"].is<bool>()) {
                r.paired = jobj["paired"].as<bool>();
            } else {
                r.paired = false;
                updateFile = true;
            }
            r.positionTracker.setTravelTime(r.travelTime);
            if (jobj["position"].is<float>() || jobj["position"].is<int>()) {
                r.positionTracker.setPosition(
                    std::clamp(jobj["position"].as<float>(), 0.0f, 100.0f));
            } else {
                r.positionTracker.setPosition(0.0f);
                updateFile = true;
            }
            r.lastPublishedPosition = r.positionTracker.getPosition();
            r.lastSavedPosition = r.positionTracker.getPosition();

            loadedRemotes.push_back(r);
        }

        if (loadedRemotes.empty()) {
            Serial.printf("No valid 1W remotes loaded from %s\n", IOHC_1W_REMOTE);
        }
        remotes = loadedRemotes;
        Serial.printf("Loaded %d x 1W remotes\n", remotes.size());
        if (updateFile) {
            this->save();
        }
        return true;
    }

   bool iohcRemote1W::save() {
        if (remotes.empty()) {
            Serial.printf("Refusing to save empty 1W remote list to %s\n", IOHC_1W_REMOTE);
            return false;
        }

        constexpr const char *tempFile = "/1W.json.tmp";
        constexpr const char *backupFile = "/1W.json.bak";
        LittleFS.remove(tempFile);
        fs::File f = LittleFS.open(tempFile, "w");
        if (!f) {
            Serial.printf("Failed to open temporary 1W settings file %s\n", tempFile);
            return false;
        }
        JsonDocument doc;
        for (const auto&r: remotes) {
            auto jobj = doc[bytesToHexString(r.node, sizeof(r.node))].to<JsonObject>();
            jobj["key"] = bytesToHexString(r.key, sizeof(r.key));

            uint8_t btmp[2];
            btmp[1] = r.sequence & 0x00ff;
            btmp[0] = r.sequence >> 8;
            jobj["sequence"] = bytesToHexString(btmp, sizeof(btmp));

            auto jarr = jobj["type"].to<JsonArray>();
            for (uint8_t i : r.type) {
                jarr.add(i);
            }

            jobj["manufacturer_id"] = r.manufacturer;
            jobj["description"] = r.description;
            jobj["name"] = r.name;
            jobj["travel_time"] = r.travelTime;
            jobj["position"] = static_cast<int>(std::round(r.positionTracker.getPosition()));
            jobj["paired"] = r.paired;
        }
        const size_t written = serializeJson(doc, f);
        f.flush();
        f.close();
        if (written == 0) {
            LittleFS.remove(tempFile);
            Serial.println("Failed to serialize 1W settings");
            return false;
        }

        LittleFS.remove(backupFile);
        if (LittleFS.exists(IOHC_1W_REMOTE) &&
            !LittleFS.rename(IOHC_1W_REMOTE, backupFile)) {
            LittleFS.remove(tempFile);
            Serial.println("Failed to back up 1W settings file");
            return false;
        }
        if (!LittleFS.rename(tempFile, IOHC_1W_REMOTE)) {
            Serial.println("Failed to replace 1W settings file");
            LittleFS.remove(tempFile);
            if (LittleFS.exists(backupFile)) {
                LittleFS.rename(backupFile, IOHC_1W_REMOTE);
            }
            return false;
        }
        LittleFS.remove(backupFile);
        return true;
    }

const std::vector<iohcRemote1W::remote>& iohcRemote1W::getRemotes() const {
    return remotes;
}

    bool iohcRemote1W::addRemote(const std::string &name) {
        remote r{};

        bool unique = false;
        while (!unique) {
            for (uint8_t i = 0; i < sizeof(r.node); i++)
                r.node[i] = esp_random() & 0xff;
            unique = std::none_of(remotes.begin(), remotes.end(), [&](const remote &e){
                return memcmp(e.node, r.node, sizeof(r.node)) == 0;
            });
        }

        for (uint8_t &b : r.key)
            b = esp_random() & 0xff;

        r.sequence = 1;
        r.type = {0, 0};
        r.manufacturer = 2;
        r.name = name;
        r.travelTime = DEFAULT_TRAVEL_TIME_SEC;
        r.paired = false;

        const char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::string desc;
        do {
            desc.clear();
            for (int i = 0; i < 4; ++i)
                desc.push_back(letters[esp_random() % 26]);
        } while (std::any_of(remotes.begin(), remotes.end(), [&](const remote &e){
            return e.description == desc;
        }));
        r.description = desc;

        r.positionTracker.setTravelTime(r.travelTime);
        remotes.push_back(r);
        nvs_write_sequence(r.node, r.sequence);
        save();
#if defined(MQTT)
        if (mqttClient.connected()) {
            std::string id = bytesToHexString(r.node, sizeof(r.node));
            std::string key = bytesToHexString(r.key, sizeof(r.key));
            publishDiscovery(id, r.name, key);
            publishTravelTimeDiscovery(id, r.name, key, r.travelTime);
            mqttClient.subscribe(("iown/" + id + "/set").c_str(), 0);
            mqttClient.subscribe(("iown/" + id + "/position/set").c_str(), 0);
            mqttClient.subscribe(("iown/" + id + "/pair").c_str(), 0);
            mqttClient.subscribe(("iown/" + id + "/add").c_str(), 0);
            mqttClient.subscribe(("iown/" + id + "/remove").c_str(), 0);
            mqttClient.subscribe(("iown/" + id + "/travel_time/set").c_str(), 0);
        }
#endif
        return true;
    }

    bool iohcRemote1W::importLearnedRemote(
        const address node, const uint8_t *key, uint16_t sequence,
        uint8_t type, uint8_t manufacturer) {
        auto it = std::find_if(remotes.begin(), remotes.end(),
                               [&](const remote &entry) {
            return memcmp(entry.node, node, sizeof(address)) == 0;
        });

        if (it == remotes.end()) {
            remote learned{};
            memcpy(learned.node, node, sizeof(address));
            memcpy(learned.key, key, sizeof(learned.key));
            learned.sequence = sequence;
            learned.type = {type, 0};
            learned.manufacturer = manufacturer;
            learned.paired = true;

            learned.travelTime = DEFAULT_TRAVEL_TIME_SEC;
            learned.positionTracker.setTravelTime(learned.travelTime);

            const std::string addressString =
                bytesToHexString(node, sizeof(address));
            learned.description = "S" + addressString;
            learned.name = "Solar " + addressString;
            remotes.push_back(learned);
            it = std::prev(remotes.end());
        } else {
            memcpy(it->key, key, sizeof(it->key));
            it->sequence = sequence;
            it->type = {type, 0};
            it->manufacturer = manufacturer;
            it->paired = true;
        }

        nvs_write_sequence(it->node, it->sequence);
        if (!save()) {
            return false;
        }

#if defined(MQTT)
        if (mqttClient.connected()) {
            const std::string id =
                bytesToHexString(it->node, sizeof(it->node));
            const std::string keyString =
                bytesToHexString(it->key, sizeof(it->key));
            publishDiscovery(id, it->name, keyString);
            publishTravelTimeDiscovery(
                id, it->name, keyString, it->travelTime);
            mqttClient.subscribe(("iown/" + id + "/set").c_str(), 0);
            mqttClient.subscribe(
                ("iown/" + id + "/position/set").c_str(), 0);
        }
#endif
        return true;
    }

    void iohcRemote1W::syncSequence(
        const address node, uint16_t nextSequence) {
        auto it = std::find_if(remotes.begin(), remotes.end(),
                               [&](const remote &entry) {
            return memcmp(entry.node, node, sizeof(address)) == 0;
        });
        if (it == remotes.end()) {
            return;
        }

        const uint16_t delta =
            static_cast<uint16_t>(nextSequence - it->sequence);
        if (delta != 0 && delta < 0x8000) {
            it->sequence = nextSequence;
            nvs_write_sequence(it->node, it->sequence);
        }
    }

    bool iohcRemote1W::removeRemote(const std::string &description) {
        auto it = std::find_if(remotes.begin(), remotes.end(), [&](const remote &e) {
            return e.description == description;
        });
        if (it == remotes.end()) {
            Serial.printf("Device %s not found\n", description.c_str());
            return false;
        }
        if (it->paired) {
            Serial.println("WARNING: Device is paired. Unpair before removing.");
            return false;
        }
        std::string id = bytesToHexString(it->node, sizeof(it->node));
#if defined(MQTT)
        if (mqttClient.connected()) {
            removeDiscovery(id);
            mqttClient.unsubscribe(("iown/" + id + "/set").c_str());
            mqttClient.unsubscribe(("iown/" + id + "/position/set").c_str());
            mqttClient.unsubscribe(("iown/" + id + "/pair").c_str());
            mqttClient.unsubscribe(("iown/" + id + "/add").c_str());
            mqttClient.unsubscribe(("iown/" + id + "/remove").c_str());
            mqttClient.unsubscribe(("iown/" + id + "/travel_time/set").c_str());
            mqttClient.publish(("iown/" + id + "/travel_time").c_str(), 0, true, "", 0);
        }
#endif
        remotes.erase(it);
        save();
        return true;
    }

    bool iohcRemote1W::renameRemote(const std::string &description, const std::string &name) {
        auto it = std::find_if(remotes.begin(), remotes.end(), [&](const remote &e) {
            return e.description == description;
        });
        if (it == remotes.end()) {
            Serial.printf("Device %s not found\n", description.c_str());
            return false;
        }
        it->name = name;
        save();
#if defined(MQTT)
        if (mqttClient.connected()) {
            std::string id = bytesToHexString(it->node, sizeof(it->node));
            std::string key = bytesToHexString(it->key, sizeof(it->key));
            publishDiscovery(id, it->name, key);
            publishTravelTimeDiscovery(id, it->name, key, it->travelTime);
        }
#endif
        return true;
    }

    void iohcRemote1W::handleRemoteAction(RemoteButton cmd, const std::string &description) {
        auto it = std::find_if(remotes.begin(), remotes.end(), [&](const remote &e) {
            return e.description == description;
        });
        if (it == remotes.end()) {
            Serial.printf("Device %s not found\n", description.c_str());
            return;
        }
        remote &r = *it;
        r.positionTracker.update();

        switch (cmd) {
            case RemoteButton::Open:
                r.positionTracker.startOpening();
                r.movement = remote::Movement::Opening;
                r.targetPosition = 100.0f;
                broadcastWebDeviceAction(r, "OPENING");
#if defined(MQTT)
                {
                    std::string id = bytesToHexString(r.node, sizeof(r.node));
                    publishCoverState(id, "OPENING");
                    publishCoverPosition(id, r.positionTracker.getPosition());
                    r.lastPublishedState = "OPENING";
                    r.lastPublishedPosition = r.positionTracker.getPosition();
                }
#endif
                break;
            case RemoteButton::Close:
                r.positionTracker.startClosing();
                r.movement = remote::Movement::Closing;
                r.targetPosition = 0.0f;
                broadcastWebDeviceAction(r, "CLOSING");
#if defined(MQTT)
                {
                    std::string id = bytesToHexString(r.node, sizeof(r.node));
                    publishCoverState(id, "CLOSING");
                    publishCoverPosition(id, r.positionTracker.getPosition());
                    r.lastPublishedState = "CLOSING";
                    r.lastPublishedPosition = r.positionTracker.getPosition();
                }
#endif
                break;
            case RemoteButton::Stop:
                r.positionTracker.stop();
                r.movement = remote::Movement::Idle;
                r.targetPosition = r.positionTracker.getPosition();
                broadcastWebDeviceAction(r, "STOP");
#if defined(MQTT)
                {
                    std::string id = bytesToHexString(r.node, sizeof(r.node));
                    publishCoverState(id, "STOP");
                    publishCoverPosition(id, r.positionTracker.getPosition());
                    r.lastPublishedState = "STOP";
                    r.lastPublishedPosition = r.positionTracker.getPosition();
                }
#endif
                break;
            default:
                break;
        }
    }

    bool iohcRemote1W::setTravelTime(const std::string &description, uint32_t travelTime) {
        auto it = std::find_if(remotes.begin(), remotes.end(), [&](const remote &e) {
            return e.description == description;
        });
        if (it == remotes.end()) {
            Serial.printf("Device %s not found\n", description.c_str());
            return false;
        }
        it->travelTime = travelTime;
        it->positionTracker.setTravelTime(travelTime);
        save();
        return true;
    }


    void iohcRemote1W::updatePositions() {
        for (auto &r : remotes) {
            r.positionTracker.update();

            float pos = r.positionTracker.getPosition();
            bool moving = r.positionTracker.isMoving();

            if (r.targetPosition >= 0.0f) {
                if (r.movement == remote::Movement::Opening && pos >= r.targetPosition) {
                    pos = r.targetPosition;
                    r.positionTracker.setPosition(pos);
                    r.positionTracker.stop();
                    moving = false;
                } else if (r.movement == remote::Movement::Closing && pos <= r.targetPosition) {
                    pos = r.targetPosition;
                    r.positionTracker.setPosition(pos);
                    r.positionTracker.stop();
                    moving = false;
                }
                if (!moving) {
                    r.targetPosition = -1.0f;
                }
            }

            if (moving) {
                display1WPosition(r.node, pos, r.name.c_str());
#if defined(MQTT) || defined(WEBSERVER)
                std::string id = bytesToHexString(r.node, sizeof(r.node));
#endif
#if defined(MQTT)
                const char *state = r.movement == remote::Movement::Opening ? "OPENING" : "CLOSING";
                if (state != r.lastPublishedState) {
                    publishCoverState(id, state);
                    r.lastPublishedState = state;
                }
#endif
#if defined(MQTT) || defined(WEBSERVER)
                if (fabs(pos - r.lastPublishedPosition) >= 1.0f) {
#if defined(MQTT)
                    publishCoverPosition(id, pos);
#endif
#if defined(WEBSERVER)
                    broadcastDevicePosition(id.c_str(), static_cast<int>(pos));
#endif
                }
                r.lastPublishedPosition = pos;
#endif
            } else {
#if defined(MQTT) || defined(WEBSERVER)
                std::string id = bytesToHexString(r.node, sizeof(r.node));
#endif
#if defined(MQTT)
                const char *state = "STOP";
                if (r.movement == remote::Movement::Opening) {
                    state = pos >= 99.5f ? "OPEN" : "STOP";
                } else if (r.movement == remote::Movement::Closing) {
                    state = pos <= 0.5f ? "CLOSE" : "STOP";
                }
                if (state != r.lastPublishedState) {
                    publishCoverState(id, state);
                    r.lastPublishedState = state;
                }
#endif
                if (r.lastPublishedPosition != pos) {
                    display1WPosition(r.node, pos, r.name.c_str());
#if defined(MQTT) || defined(WEBSERVER)
                    if (fabs(pos - r.lastPublishedPosition) >= 1.0f) {
#if defined(MQTT)
                        publishCoverPosition(id, pos);
#endif
#if defined(WEBSERVER)
                        broadcastDevicePosition(id.c_str(), static_cast<int>(pos));
#endif
                    }
#endif
                    r.lastPublishedPosition = pos;
                }
                r.movement = remote::Movement::Idle;
                if (fabs(pos - r.lastSavedPosition) >= 1.0f) {
                    save();
                    r.lastSavedPosition = pos;
                }
            }
        }
    }
}
