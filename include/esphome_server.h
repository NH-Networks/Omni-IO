/*
 * SPDX-FileCopyrightText: 2026 CloudAXS
 * SPDX-License-Identifier: LicenseRef-CloudAXS-Proprietary
 */
#ifndef ESPHOME_SERVER_H
#define ESPHOME_SERVER_H

#include <user_config.h>

#if defined(ESPHOME_API)

#include <string>
#include <vector>
#include <cstdint>

namespace EspHomeMsg {
    constexpr uint16_t HELLO_REQUEST = 1;
    constexpr uint16_t HELLO_RESPONSE = 2;
    constexpr uint16_t CONNECT_REQUEST = 3;
    constexpr uint16_t CONNECT_RESPONSE = 4;
    constexpr uint16_t DISCONNECT_REQUEST = 5;
    constexpr uint16_t DISCONNECT_RESPONSE = 6;
    constexpr uint16_t PING_REQUEST = 7;
    constexpr uint16_t PING_RESPONSE = 8;
    constexpr uint16_t DEVICE_INFO_REQUEST = 9;
    constexpr uint16_t DEVICE_INFO_RESPONSE = 10;
    constexpr uint16_t LIST_ENTITIES_REQUEST = 11;
    constexpr uint16_t LIST_ENTITIES_BINARY_SENSOR_RESPONSE = 12;
    constexpr uint16_t LIST_ENTITIES_COVER_RESPONSE = 13;
    constexpr uint16_t LIST_ENTITIES_FAN_RESPONSE = 14;
    constexpr uint16_t LIST_ENTITIES_LIGHT_RESPONSE = 15;
    constexpr uint16_t LIST_ENTITIES_SENSOR_RESPONSE = 16;
    constexpr uint16_t LIST_ENTITIES_SWITCH_RESPONSE = 17;
    constexpr uint16_t LIST_ENTITIES_TEXT_SENSOR_RESPONSE = 18;
    constexpr uint16_t LIST_ENTITIES_DONE_RESPONSE = 19;
    constexpr uint16_t SUBSCRIBE_STATES_REQUEST = 20;
    constexpr uint16_t BINARY_SENSOR_STATE_RESPONSE = 21;
    constexpr uint16_t COVER_STATE_RESPONSE = 22;
    constexpr uint16_t SENSOR_STATE_RESPONSE = 25;
    constexpr uint16_t SWITCH_STATE_RESPONSE = 26;
    constexpr uint16_t TEXT_SENSOR_STATE_RESPONSE = 27;
    constexpr uint16_t SUBSCRIBE_LOGS_REQUEST = 28;
    constexpr uint16_t COVER_COMMAND_REQUEST = 30;
    constexpr uint16_t SUBSCRIBE_HOMEASSISTANT_SERVICES_REQUEST = 34;
    constexpr uint16_t GET_TIME_REQUEST = 36;
    constexpr uint16_t GET_TIME_RESPONSE = 37;
    constexpr uint16_t SUBSCRIBE_HOME_ASSISTANT_STATES_REQUEST = 38;
    constexpr uint16_t LIST_ENTITIES_NUMBER_RESPONSE = 49;
    constexpr uint16_t NUMBER_STATE_RESPONSE = 50;
    constexpr uint16_t NUMBER_COMMAND_REQUEST = 51;
    constexpr uint16_t LIST_ENTITIES_SELECT_RESPONSE = 52;
    constexpr uint16_t SELECT_STATE_RESPONSE = 53;
    constexpr uint16_t SELECT_COMMAND_REQUEST = 54;
    constexpr uint16_t LIST_ENTITIES_BUTTON_RESPONSE = 61;
    constexpr uint16_t BUTTON_COMMAND_REQUEST = 62;
}

// Computes 32-bit FNV-1a hash to generate stable entity keys for Home Assistant
uint32_t espHomeFnv1a(const std::string &str);

// Server lifecycle
void initEspHomeServer();
void startEspHomeServer();
void stopEspHomeServer();
bool isEspHomeServerRunning();
uint16_t getEspHomeConnectedClients();

// State notification functions called by Omni-IO core
void notifyEspHomeCoverPosition(const std::string &id, float position);
void notifyEspHomeCoverState(const std::string &id, const char *state);
void notifyEspHomeTravelTime(const std::string &id, uint32_t travelTime);
void notifyEspHomeDiagnostics();
void syncEspHomeDevices();

#else

// Stubs when ESPHOME_API is not compiled
inline void initEspHomeServer() {}
inline void startEspHomeServer() {}
inline void stopEspHomeServer() {}
inline bool isEspHomeServerRunning() { return false; }
inline uint16_t getEspHomeConnectedClients() { return 0; }
inline void notifyEspHomeCoverPosition(const std::string &, float) {}
inline void notifyEspHomeCoverState(const std::string &, const char *) {}
inline void notifyEspHomeTravelTime(const std::string &, uint32_t) {}
inline void notifyEspHomeDiagnostics() {}
inline void syncEspHomeDevices() {}

#endif // ESPHOME_API

#endif // ESPHOME_SERVER_H

