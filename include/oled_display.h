/*
 * Modifications Copyright 2026 CloudAXS.
 * Original upstream portions remain licensed under Apache-2.0.
 */
#pragma once
#include <Arduino.h>
#include <user_config.h>
#include <board-config.h>

#if defined(SSD1306_DISPLAY)

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C
#define OLED_RST DISPLAY_OLED_RST_PIN
#define OLED_SDA I2C_SDA_PIN
#define OLED_SCL I2C_SCL_PIN

void display1WAction(const uint8_t *remote, const char *action, const char *dir, const char *name = nullptr);
void display1WPosition(const uint8_t *remote, float position, const char *name = nullptr);
void displayCustomMessage(const char* message, const char* status = nullptr);
void clearDisplayMessages();
void updateDisplayStatus();
void wakeDisplay();
void setDiscoveryDisplay(int seconds);

bool initDisplay();
bool isDisplayEnabled();
void setDisplayEnabled(bool enabled);

// Screensaver timeout (seconds of inactivity before screensaver, default 60)
uint16_t getScreensaverTimeout();
void setScreensaverTimeout(uint16_t seconds);

// Screen-off timeout (seconds of inactivity before display off, default 3600)
uint16_t getScreenOffTimeout();
void setScreenOffTimeout(uint16_t seconds);

// Dim level: 0 = low (contrast 1), 1 = medium (contrast 64), 2 = high (contrast 200)
uint8_t getDimLevel();
void setDimLevel(uint8_t level);

// Show CPU temperature in display header
bool isCpuTempEnabled();
void setCpuTempEnabled(bool enabled);

#else

inline void display1WAction(const uint8_t * = nullptr, const char * = nullptr, const char * = nullptr, const char * = nullptr) {}
inline void display1WPosition(const uint8_t * = nullptr, float = 0, const char * = nullptr) {}
inline void displayCustomMessage(const char* = nullptr, const char* = nullptr) {}
inline void clearDisplayMessages() {}
inline void updateDisplayStatus() {}
inline void wakeDisplay() {}
inline void setDiscoveryDisplay(int = 0) {}

inline bool initDisplay() { return true; }
inline bool isDisplayEnabled() { return false; }
inline void setDisplayEnabled(bool) {}

inline uint16_t getScreensaverTimeout() { return 60; }
inline void setScreensaverTimeout(uint16_t) {}

inline uint16_t getScreenOffTimeout() { return 3600; }
inline void setScreenOffTimeout(uint16_t) {}

inline uint8_t getDimLevel() { return 0; }
inline void setDimLevel(uint8_t) {}

inline bool isCpuTempEnabled() { return false; }
inline void setCpuTempEnabled(bool) {}

#endif

