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
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

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
const int SECONDS_BEFORE_SCREENSAVER = 60;
const int SECONDS_BEFORE_SCREEN_OFF = 3600; // 60 minutes

const uint8_t PROGMEM miopenioLogo[] =
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

const uint8_t PROGMEM mqttIcons[3][2*7] = {
     {
        B00111000, B11100000,
        B01100000, B00110000,
        B11000000, B00011000,
        B01100000, B00110000,
        B00111000, B11100000,
    }, // empty chain ends
    {
        B00111000, B11100000,
        B01100000, B00110000,
        B11001111, B10011000,
        B01100000, B00110000,
        B00111000, B11100000,
    }, // filled chain ends
    {
        B00011100, B00111000,
        B01100011, B00001100,
        B11000000, B00001100,
        B11000011, B00011000,
        B01110000, B11100000,
    }, // broken chain ends
};

int mqttStatusToIconIndex() {
    switch (mqttStatus) {
    case ConnState::Connecting:
        return 0;
    case ConnState::Connected:
        return 1;
    case ConnState::Disconnected:
        return 2;
    };
    return 2;
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

    Wire.begin(OLED_SDA, OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        return false;
    }
    
    // Dim the screen to reduce power consumption and heat
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(1); // Set contrast to a low value instead of 0

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

const char* getRemoteName(const uint8_t *remote, const char *name) {
    if (name) return name;
    
    const auto *entry = IOHC::iohcRemoteMap::getInstance()->find(remote);
    if (entry) return entry->name.c_str();

    return bytesToHexString(remote, 3).c_str();
}

void display1WAction(const uint8_t *remote, const char *action, const char *dir, const char *name) {
    displayCustomMessage(format("%s: %s", dir, getRemoteName(remote, name)).c_str(), action);
}

void display1WPosition(const uint8_t *remote, float position, const char *name) {
    displayCustomMessage(getRemoteName(remote, name), format("%d%%", static_cast<int>(position)).c_str());
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
    // miopenio logo is 16x10
    display.drawBitmap(x+1, y+1, miopenioLogo, 16, 10, SSD1306_WHITE);
    display.setCursor(x+20, y+4);
    display.print("MiOpen.IO");
}

void drawHeader() {
    drawLogo(0, 0);

#if defined(MQTT)
    // mqtt icon is 16x5 (including 3 pixels space, so adding 1 extra for a reasonable space)
    const auto mqttIcon = mqttIcons[mqttStatusToIconIndex()];
    display.drawBitmap(127-8-1-16, 5, mqttIcon, 16, 5, SSD1306_WHITE);
#endif // MQTT

    // wifi icon is 8x7
    const auto wifiIcon = wifiIcons[min(wifiStatus.signalStrengthPercent.load(), 99) / 25];
    display.drawBitmap(127-8, 3, wifiIcon, 8, 7, SSD1306_WHITE);

    // CPU Temperature
    display.setCursor(82, 4);
    display.printf("%.0fC", temperatureRead());
}

void drawFooter() {
    if (wifiStatus.connectionStatus == ConnState::Connected) {
        display.setCursor(1, 56);
        // every 10 seconds alternate between url and ip
        if (getSecondsSinceStart() / 10 % 2 == 0) {
            display.println("http://miopenio.local");
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
    // draw logo and text at random position to avoid burn-in
    const int x = 50.0 * std::rand() / RAND_MAX; // number between 0 and 50
    const int y = 30.0 * std::rand() / RAND_MAX; // adjusted max y to 30 to fit text
    drawLogo(x, y);
    
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(x, y + 16);
    display.printf("CPU: %.0fC", temperatureRead());
    
    if (wifiStatus.connectionStatus == ConnState::Connected) {
        display.setCursor(x, y + 26);
        display.print(WiFi.localIP().toString().c_str());
    }
}

void displayTask(void *) {
    bool taskDisplayOn = true;
    bool isDimmed = true;
    
    // Dirty tracking state
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

        if (showData || secondsSinceNoData < SECONDS_BEFORE_SCREENSAVER) {
            std::vector<std::string> currentLines;
            
            // Check dirty conditions for data screen
            bool dirty = false;
            if (isDimmed) dirty = true; // Needs undimming
            if (now != lastDrawnTime) dirty = true;
            if (wifiStatus.rssi != lastRssi) dirty = true;
#if defined(MQTT)
            int currentMqttIcon = mqttStatusToIconIndex();
            if (currentMqttIcon != lastMqttIcon) dirty = true;
#endif

            // To avoid flickering and I2C spam, we only want to redraw if data is dirty, 
            // but we must call drawContents to get the lines and check.
            // A simpler way: we prepare a dummy call to see if lines changed? No, 
            // we will just call getTextToDisplay directly.
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
        } else if (secondsSinceNoData < SECONDS_BEFORE_SCREEN_OFF) {
            setTimerSpeed(slow);
            if (!taskDisplayOn) {
                display.ssd1306_command(SSD1306_DISPLAYON);
                taskDisplayOn = true;
            }
            if (!isDimmed) {
                display.ssd1306_command(SSD1306_SETCONTRAST);
                display.ssd1306_command(1);
                isDimmed = true;
            }
            // Screensaver is drawn every SLOW tick (5000ms), so it's fine to redraw
            display.clearDisplay();
            drawLogo();
            display.display();
            
            // Reset dirty tracking for when we wake up
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
