#pragma once
#if defined(SSD1306_DISPLAY)
#include <Arduino.h>

void display1WAction(const uint8_t *remote, const char *action, const char *dir, const char *name);
void display1WPosition(const uint8_t *remote, float position, const char *name);
void displayCustomMessage(const char* message, const char* status);
void clearDisplayMessages();
void updateDisplayStatus();
void wakeDisplay();
void setDiscoveryDisplay(int seconds);
void broadcastLog(const String &msg);

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

// No-op stubs when no display is present
#include <cstdint>

inline void display1WAction(const uint8_t *, const char *, const char *, const char *) {}
inline void display1WPosition(const uint8_t *, float, const char *) {}
inline void displayCustomMessage(const char *, const char *) {}
inline void clearDisplayMessages() {}
inline void updateDisplayStatus() {}
inline void wakeDisplay() {}
inline void setDiscoveryDisplay(int) {}

// broadcastLog takes const char* to avoid pulling in Arduino String outside display TU
inline void broadcastLog(const char *) {}

inline bool initDisplay() { return false; }
inline bool isDisplayEnabled() { return false; }
inline void setDisplayEnabled(bool) {}

inline uint16_t getScreensaverTimeout() { return 60; }
inline void setScreensaverTimeout(uint16_t) {}

inline uint16_t getScreenOffTimeout() { return 3600; }
inline void setScreenOffTimeout(uint16_t) {}

inline uint8_t getDimLevel() { return 1; }
inline void setDimLevel(uint8_t) {}

inline bool isCpuTempEnabled() { return false; }
inline void setCpuTempEnabled(bool) {}

#endif
