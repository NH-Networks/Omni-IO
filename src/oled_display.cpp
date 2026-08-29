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

#include <user_config.h>
#if defined(SSD1306_DISPLAY)
#include <board-config.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <oled_display.h>
#include <iohcCryptoHelpers.h>
#include <iohcRemoteMap.h>
#include <interact.h>
#include <wifi_helper.h>
#include <WiFi.h>
#include <display_helpers.h>
#include <nvs_helpers.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#if defined(ESPHOME_API)
#include <esphome_server.h>
#endif

// OLED screen dimensions
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64

// Map board I2C pins to OLED names
#define OLED_SDA     I2C_SDA_PIN
#define OLED_SCL     I2C_SCL_PIN
#define OLED_RST     DISPLAY_OLED_RST_PIN
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);
DisplayBuffer displayBuffer;
SemaphoreHandle_t displayBufferMutex = xSemaphoreCreateMutex();

TaskHandle_t displayTaskHandle = nullptr;
std::chrono::time_point<std::chrono::system_clock> startTime;
std::atomic<int64_t> lastDataTime = 0;
std::atomic<bool> displayEnabled = true;
std::atomic<int64_t> discoveryEndMicros{0};
void displayTask(void *);

const int MILLIS_BETWEEN_DISPLAY_UPDATE_SLOW = 5000;
const int MILLIS_BETWEEN_DISPLAY_UPDATE_FAST = 100;

// Runtime-configurable display settings
static std::atomic<uint16_t> screensaverTimeout{60};    // seconds
static std::atomic<uint16_t> screenOffTimeout{3600};    // seconds
static std::atomic<uint8_t>  dimLevel{0};               // 0=low,1=med,2=high
static std::atomic<bool>     cpuTempEnabled{true};

// Map dim level to SSD1306 contrast byte
static uint8_t dimLevelToContrast(uint8_t level) {
    switch (level) {
        case 2:  return 200;
        case 1:  return 64;
        default: return 1;
    }
}

uint16_t getScreensaverTimeout() { return screensaverTimeout.load(); }
void setScreensaverTimeout(uint16_t seconds) {
    screensaverTimeout = seconds;
    nvs_write_u16(NVS_KEY_DISPLAY_SCREENSAVER, seconds);
}

uint16_t getScreenOffTimeout() { return screenOffTimeout.load(); }
void setScreenOffTimeout(uint16_t seconds) {
    screenOffTimeout = seconds;
    nvs_write_u16(NVS_KEY_DISPLAY_SCREENOFF, seconds);
}

uint8_t getDimLevel() { return dimLevel.load(); }
void setDimLevel(uint8_t level) {
    if (level > 2) level = 2;
    dimLevel = level;
    nvs_write_u16(NVS_KEY_DISPLAY_DIM, level);
}

bool isCpuTempEnabled() { return cpuTempEnabled.load(); }
void setCpuTempEnabled(bool enabled) {
    cpuTempEnabled = enabled;
    nvs_write_bool(NVS_KEY_DISPLAY_CPUTEMP, enabled);
}

const uint8_t PROGMEM omniIoLogo[] =
{
    B00000010, B00000000,
    B00001101, B10000000,
    B00110000, B01100000,
    B11000000, B00011000,
    B01001011, B10010000,
    B01001010, B10010000,
    B01001010, B10010000,
    B01001011, B10010000,
    B01000000, B00010000,
    B01111111, B11110000,
}; // House with IO in it

const uint8_t PROGMEM wifiIcons[4][7] = {
    {
        B00000000,
        B00000000,
        B00000000,
        B00000000,
        B00000000,
        B00000000,
        B11011011,
    }, // 3 empty bars
    {
        B00000000,
        B00000000,
        B00000000,
        B00000000,
        B11000000,
        B11000000,
        B11011011,
    }, // first bar filled
    {
        B00000000,
        B00000000,
        B00011000,
        B00011000,
        B11011000,
        B11011000,
        B11011011,
    }, // first two filled
    {
        B00000011,
        B00000011,
        B00011011,
        B00011011,
        B11011011,
        B11011011,
        B11011011,
    }, // all three filled
};

const uint8_t PROGMEM mqttIcons[3][10] = {
    {
        B00111000, B11100000,
        B01100000, B00110000,
        B11000000, B00011000,
        B01100000, B00110000,
        B00111000, B11100000,
    }, // empty / connecting chain ends
    {
        B00111000, B11100000,
        B01100000, B00110000,
        B11001111, B10011000,
        B01100000, B00110000,
        B00111000, B11100000,
    }, // connected / filled chain ends
    {
        B00011100, B00111000,
        B01100011, B00001100,
        B11000000, B00001100,
        B11000011, B00011000,
        B01110000, B11100000,
    }, // disconnected / broken chain ends
};

#if defined(ESPHOME_API)
// 11x7 bitmaps: ESPHome / Home Assistant status icon
// Index 0: Waiting / listening (outline house with chimney)
// Index 1: Connected (solid house with chimney & door)
const uint8_t PROGMEM espHomeIcons[2][14] = {
    {
        B00001100, B10000000, // ....##..#.. (chimney on right)
        B00010010, B10000000, // ...#..#.#..
        B00100001, B10000000, // ..#....##..
        B01111111, B11000000, // .#########. (roof eave)
        B00100000, B10000000, // ..#.....#.. (walls)
        B00100000, B10000000, // ..#.....#..
        B00111111, B10000000, // ..#######.. (base)
    },
    {
        B00001100, B10000000, // ....##..#.. (chimney on right)
        B00011110, B10000000, // ...####.#..
        B00111111, B10000000, // ..#######..
        B01111111, B11000000, // .#########. (roof eave)
        B00110001, B10000000, // ..##...##.. (walls + door opening)
        B00110001, B10000000, // ..##...##..
        B00111111, B10000000, // ..#######.. (base)
    },
};
#endif

int mqttStatusToIconIndex() {
    switch (mqttStatus) {
    case ConnState::Connecting:
        return 0;
    case ConnState::Connected:
        return 1;
    case ConnState::Disconnected:
    default:
        return 2;
    };
}

const bool fast = true;
const bool slow = false;
std::atomic<bool> timerIsFast = false;

static void notifyDisplayTask() {
    if (displayTaskHandle != nullptr) {
        xTaskNotifyGive(displayTaskHandle);
    }
}

void setTimerSpeed(bool needsFast) {
    if (needsFast != timerIsFast) {
        timerIsFast = needsFast;
        notifyDisplayTask();
    }
}

bool initDisplay() {
    bool enabled = true;
    if (nvs_read_bool(NVS_KEY_DISPLAY_ENABLED, enabled)) {
        displayEnabled = enabled;
    }

    uint16_t ssSecs = 60;
    if (nvs_read_u16(NVS_KEY_DISPLAY_SCREENSAVER, ssSecs)) {
        screensaverTimeout = ssSecs;
    }

    uint16_t offSecs = 3600;
    if (nvs_read_u16(NVS_KEY_DISPLAY_SCREENOFF, offSecs)) {
        screenOffTimeout = offSecs;
    }

    uint16_t dim = 0;
    if (nvs_read_u16(NVS_KEY_DISPLAY_DIM, dim)) {
        dimLevel = static_cast<uint8_t>(dim > 2 ? 0 : dim);
    }

    bool showTemp = true;
    if (nvs_read_bool(NVS_KEY_DISPLAY_CPUTEMP, showTemp)) {
        cpuTempEnabled = showTemp;
    }

    Wire.begin(OLED_SDA, OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        return false;
    }

    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(1);

    if (xTaskCreatePinnedToCore(displayTask, "DisplayTask", 4096, nullptr, 1,
                                &displayTaskHandle, tskNO_AFFINITY) != pdPASS) {
        Serial.println("Failed to create display task");
        return false;
    }

    startTime = std::chrono::system_clock::now();
    lastDataTime = esp_timer_get_time();
    xTaskNotifyGive(displayTaskHandle);

    return true;
}

int getSecondsSince(std::chrono::time_point<std::chrono::system_clock> &time) {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - time).count();
}

int getSecondsSinceStart() {
    return getSecondsSince(startTime);
}

int getSecondsSinceNoData() {
    return (esp_timer_get_time() - lastDataTime.load()) / 1000000LL;
}

// Returns a stable std::string to avoid dangling-pointer when falling back to hex
static std::string getRemoteName(const uint8_t *remote, const char *name) {
    if (name) return std::string(name);

    const auto *entry = IOHC::iohcRemoteMap::getInstance()->find(remote);
    if (entry) return entry->name;

    return bytesToHexString(remote, 3);
}

void display1WAction(const uint8_t *remote, const char *action, const char *dir, const char *name) {
    char buf[64];
    const std::string remoteName = getRemoteName(remote, name);
    snprintf(buf, sizeof(buf), "%s: %s", dir, remoteName.c_str());
    displayCustomMessage(buf, action);
}

void display1WPosition(const uint8_t *remote, float position, const char *name) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(position));
    const std::string remoteName = getRemoteName(remote, name);
    displayCustomMessage(remoteName.c_str(), buf);
}

void displayCustomMessage(const char* message, const char* status) {
    if (!displayEnabled.load()) {
        return;
    }

    xSemaphoreTake(displayBufferMutex, portMAX_DELAY);
    displayBuffer.addLine(message, status ? status : "");
    xSemaphoreGive(displayBufferMutex);

    setTimerSpeed(fast);
    notifyDisplayTask();
}

void clearDisplayMessages() {
    xSemaphoreTake(displayBufferMutex, portMAX_DELAY);
    displayBuffer.clear();
    xSemaphoreGive(displayBufferMutex);

    lastDataTime.store(esp_timer_get_time());
    setTimerSpeed(slow);
}

void updateDisplayStatus() {
    if (!displayEnabled.load()) {
        return;
    }

    setTimerSpeed(fast);
    notifyDisplayTask();
}

void wakeDisplay() {
    if (!displayEnabled.load()) return;
    lastDataTime.store(esp_timer_get_time());
    setTimerSpeed(fast);
    notifyDisplayTask();
}

void setDiscoveryDisplay(int seconds) {
    if (!displayEnabled.load()) return;
    discoveryEndMicros.store(esp_timer_get_time() + seconds * 1000000LL);
    wakeDisplay();
}

bool isDisplayEnabled() {
    return displayEnabled.load();
}

void setDisplayEnabled(bool enabled) {
    if (enabled == displayEnabled.load()) {
        return;
    }

    displayEnabled = enabled;
    nvs_write_bool(NVS_KEY_DISPLAY_ENABLED, enabled);

    if (!enabled) {
        xSemaphoreTake(displayBufferMutex, portMAX_DELAY);
        displayBuffer.clear();
        xSemaphoreGive(displayBufferMutex);
        lastDataTime.store(esp_timer_get_time());
        setTimerSpeed(slow);
    } else {
        lastDataTime.store(esp_timer_get_time());
        setTimerSpeed(fast);
    }

    notifyDisplayTask();
}

void drawLogo(int x, int y) {
    display.drawBitmap(x+1, y+1, omniIoLogo, 16, 10, SSD1306_WHITE);
    display.setCursor(x+20, y+4);
    display.print("Omni-IO");
}

void drawHeader() {
    drawLogo(0, 0);

#if defined(MQTT)
    if (mqtt_enabled) {
        const auto mqttIcon = mqttIcons[mqttStatusToIconIndex()];
        display.drawBitmap(127-8-1-16, 5, mqttIcon, 16, 5, SSD1306_WHITE);
    }
#endif

#if defined(ESPHOME_API)
    if (esphome_enabled && isEspHomeServerRunning()) {
        int iconIdx = (getEspHomeConnectedClients() > 0) ? 1 : 0;
        const auto espIcon = espHomeIcons[iconIdx];
        int xPos = (mqtt_enabled) ? (127 - 8 - 1 - 16 - 2 - 11) : (127 - 8 - 3 - 11);
        display.drawBitmap(xPos, 3, espIcon, 11, 7, SSD1306_WHITE);
    }
#endif

    const auto wifiIcon = wifiIcons[min(wifiStatus.signalStrengthPercent.load(), 99) / 25];
    display.drawBitmap(127-8, 3, wifiIcon, 8, 7, SSD1306_WHITE);

    if (cpuTempEnabled.load()) {
        int cpuX = (mqtt_enabled && esphome_enabled && isEspHomeServerRunning()) ? 62 : 74;
        display.setCursor(cpuX, 4);
        display.printf("%.0fC", temperatureRead());
    }
}

void drawFooter() {
    if (wifiStatus.connectionStatus == ConnState::Connected) {
        display.setCursor(1, 56);
        if (getSecondsSinceStart() / 10 % 2 == 0) {
            display.println("http://omni-io.local");
        } else {
            display.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        }
    }
}

bool drawContents(const std::vector<std::string>& inLines) {
    for(auto &line : inLines) {
        display.println(line.c_str());
    }
    return inLines.size() > 0;
}

void drawData(const std::vector<std::string>& currentLines) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    drawHeader();

    if (esp_timer_get_time() < discoveryEndMicros.load()) {
        display.setCursor(10, 25);
        display.setTextSize(2);
        display.println("DISCOVERY");
        display.setCursor(10, 45);
        display.setTextSize(1);
        display.println("Mode Active...");
        setTimerSpeed(fast);
    } else {
        display.setCursor(0, 20);
        const bool hasData = drawContents(currentLines);
        if (!hasData) {
            setTimerSpeed(slow);
        }
    }

    drawFooter();
}

void drawLogo() {
    const int x = 50.0 * std::rand() / RAND_MAX;
    const int y = 30.0 * std::rand() / RAND_MAX;
    drawLogo(x, y);

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    if (cpuTempEnabled.load()) {
        display.setCursor(x, y + 16);
        display.printf("CPU: %.0fC", temperatureRead());
    }

    if (wifiStatus.connectionStatus == ConnState::Connected) {
        display.setCursor(x, y + (cpuTempEnabled.load() ? 26 : 16));
        display.print(WiFi.localIP().toString().c_str());
    }
}

void displayTask(void *) {
    bool taskDisplayOn = true;
    bool isDimmed = true;

    time_t lastDrawnTime = 0;
    int lastRssi = 0;
    int lastMqttIcon = -1;
    std::vector<std::string> lastLines;

    while (true) {
        const bool showData = timerIsFast.load();
        const TickType_t waitTicks = pdMS_TO_TICKS(showData ? MILLIS_BETWEEN_DISPLAY_UPDATE_FAST
                                                            : MILLIS_BETWEEN_DISPLAY_UPDATE_SLOW);
        ulTaskNotifyTake(pdTRUE, waitTicks);

        if (displayEnabled.load()) {
            if (!taskDisplayOn) {
                display.ssd1306_command(SSD1306_DISPLAYON);
                taskDisplayOn = true;
            }
        } else {
            if (taskDisplayOn) {
                display.clearDisplay();
                display.display();
                display.ssd1306_command(SSD1306_DISPLAYOFF);
                taskDisplayOn = false;
            }
            continue;
        }

        const auto secondsSinceNoData = getSecondsSinceNoData();
        time_t now = time(nullptr);

        const int ssSecs  = screensaverTimeout.load();
        const int offSecs = screenOffTimeout.load();

        if (showData || secondsSinceNoData < ssSecs) {
            std::vector<std::string> currentLines;

            bool dirty = false;
            if (isDimmed) dirty = true;
            if (now != lastDrawnTime) dirty = true;
            if (wifiStatus.rssi != lastRssi) dirty = true;
#if defined(MQTT)
            int currentMqttIcon = !mqtt_enabled ? -2 : mqttStatusToIconIndex();
            if (currentMqttIcon != lastMqttIcon) dirty = true;
#endif

            int width = SCREEN_WIDTH / 6;
            int height = (SCREEN_HEIGHT - 20 - 8) / 8;
            xSemaphoreTake(displayBufferMutex, portMAX_DELAY);
            currentLines = displayBuffer.getTextToDisplay(width, height);
            xSemaphoreGive(displayBufferMutex);

            if (currentLines != lastLines) dirty = true;

            if (dirty) {
                if (isDimmed) {
                    display.dim(false);
                    isDimmed = false;
                }
                display.clearDisplay();
                drawData(currentLines);
                display.display();

                lastDrawnTime = now;
                lastRssi = wifiStatus.rssi;
#if defined(MQTT)
                lastMqttIcon = currentMqttIcon;
#endif
                lastLines = currentLines;
            }
        } else if (offSecs == 0 || secondsSinceNoData < offSecs) {
            setTimerSpeed(slow);
            if (!taskDisplayOn) {
                display.ssd1306_command(SSD1306_DISPLAYON);
                taskDisplayOn = true;
            }
            if (!isDimmed) {
                const uint8_t contrast = dimLevelToContrast(dimLevel.load());
                display.ssd1306_command(SSD1306_SETCONTRAST);
                display.ssd1306_command(contrast);
                isDimmed = true;
            }
            display.clearDisplay();
            drawLogo();
            display.display();

            lastDrawnTime = 0;
            lastLines.clear();
        } else {
            setTimerSpeed(slow);
            if (taskDisplayOn) {
                display.clearDisplay();
                display.display();
                display.ssd1306_command(SSD1306_DISPLAYOFF);
                taskDisplayOn = false;
            }
            continue;
        }
    }
}

#endif
