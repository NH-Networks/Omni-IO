#pragma once
#if defined(SSD1306_DISPLAY)

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

#endif
