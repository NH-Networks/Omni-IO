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

#include <wifi_helper.h>
#include <oled_display.h>
#include <user_config.h>
#include <log_buffer.h>
#include <nvs_helpers.h>
#if defined(MQTT)
#include <mqtt_handler.h>
#endif
#if defined(SYSLOG)
#include <syslog_helper.h>
#endif
#if defined(ESPHOME_API)
#include <esphome_server.h>
#endif
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <TickerUsESP32.h>
#include <tuple>

const long PORTAL_TIMEOUT = 300000; // 5 minuten = 300.000 ms
const uint32_t WIFI_NOTIFY_GOT_IP = BIT0;
const uint32_t WIFI_NOTIFY_DISCONNECTED = BIT1;
const uint32_t WIFI_NOTIFY_RECONNECT = BIT2;
static constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 10000;
static constexpr uint32_t WIFI_STACK_RESET_AFTER_MS = 120000;
static constexpr uint32_t WIFI_RESTART_AFTER_MS = 1800000;

// below variables are thread safe because of the use of a single task that reads/modifies them (except for wifiStatus, but that one has atomic fields)
TimersUS::TickerUsESP32 wifiReconnectTimer {};
TimersUS::TickerUsESP32 rssiTimer {};
WiFiStatus wifiStatus = { ConnState::Disconnected, 0, 0 };

TaskHandle_t wifiWorkerTaskHandle = NULL;
bool mdnsStarted = false;
bool webServerStarted = false;
static uint32_t s_wifiDisconnectedSinceMs = 0;
static uint32_t s_lastWiFiReconnectAttemptMs = 0;
static uint32_t s_lastWiFiStackResetMs = 0;
static uint16_t s_wifiReconnectAttempts = 0;

// Replicate WiFiManager::getRSSIasQuality() without constructing a WiFiManager object.
static int rssiToQuality(int rssi) {
    if (rssi <= -100) return 0;
    if (rssi >= -50)  return 100;
    return 2 * (rssi + 100);
}

static void notifyWiFiWorker(uint32_t bits) {
    if (wifiWorkerTaskHandle != NULL) {
        xTaskNotify(wifiWorkerTaskHandle, bits, eSetBits);
    }
}

static void rssiTimerCb() {
    if (WiFi.status() == WL_CONNECTED) {
        int newRssi = WiFi.RSSI();
        wifiStatus.rssi = newRssi;
        wifiStatus.signalStrengthPercent = rssiToQuality(newRssi);
#if defined(ESPHOME_API)
        static uint32_t lastEspHomeDiagMs = 0;
        static int lastSentRssi = 999;
        uint32_t now = millis();
        if (now - lastEspHomeDiagMs >= 30000UL || std::abs(newRssi - lastSentRssi) >= 5) {
            lastEspHomeDiagMs = now;
            lastSentRssi = newRssi;
            notifyEspHomeDiagnostics();
        }
#endif
    }
}

static void wifiReconnectTimerCb() {
    notifyWiFiWorker(WIFI_NOTIFY_RECONNECT);
}

static void ensureWebServerStarted() {
    if (!webServerStarted) {
        setupWebServer();
        webServerStarted = true;
    }
}

static void onMqttAfterWifi() {
#if defined(MQTT)
        // Establish MQTT connection if needed and MQTT client is initialized
        if (!mqttClient.connected() && mqttStatus != ConnState::Connecting) {
            connectToMqtt();
        }
#endif
}

static void handleWifiConnected() {
    if (WiFi.status() == WL_CONNECTED) {
        wifiStatus.connectionStatus = ConnState::Connected;
        wifiStatus.rssi = WiFi.RSSI();
        wifiStatus.signalStrengthPercent = rssiToQuality(wifiStatus.rssi);
        s_wifiDisconnectedSinceMs = 0;
        s_lastWiFiReconnectAttemptMs = 0;
        s_lastWiFiStackResetMs = 0;
        s_wifiReconnectAttempts = 0;

        if (WiFi.getMode() == WIFI_AP_STA) {
            Serial.println("WiFi: connected, disabling fallback AP mode");
            WiFi.mode(WIFI_STA);
        }

        static std::string sntp_server;
        sntp_server = "pool.ntp.org";
        nvs_read_string(NVS_KEY_NET_SNTP, sntp_server);
        configTime(0, 0, sntp_server.c_str());

        std::string tz = "CET-1CEST,M3.5.0,M10.5.0/3"; // Default: Europe/Amsterdam
        if (!nvs_read_string(NVS_KEY_NET_TZ, tz) || tz.empty()) {
            tz = "CET-1CEST,M3.5.0,M10.5.0/3";
        }
        setenv("TZ", tz.c_str(), 1);
        tzset();

        wifiReconnectTimer.detach();
        rssiTimer.attach(5, rssiTimerCb);
        updateDisplayStatus();

        if (!mdnsStarted) {
            if (!MDNS.begin("omni-io")) {
                Serial.println("WiFi: mDNS start failed");
            } else {
                mdnsStarted = true;
                Serial.println("WiFi: mDNS started at http://omni-io.local");
            }
        }

#if defined(ESPHOME_API)
        if (mdnsStarted && esphome_enabled) {
            MDNS.addService("esphomelib", "tcp", esphome_port);
            MDNS.addServiceTxt("esphomelib", "tcp", "version", "2024.9.0");
            MDNS.addServiceTxt("esphomelib", "tcp", "mac", WiFi.macAddress());
            MDNS.addServiceTxt("esphomelib", "tcp", "platform", "ESP32");
            MDNS.addServiceTxt("esphomelib", "tcp", "board", "esp32");
            MDNS.addServiceTxt("esphomelib", "tcp", "network", "wifi");
            MDNS.addServiceTxt("esphomelib", "tcp", "friendly_name", esphome_name.empty() ? "Omni-IO Gateway" : esphome_name.c_str());
            MDNS.addServiceTxt("esphomelib", "tcp", "project_name", "NH-Networks.Omni-IO");
            MDNS.addServiceTxt("esphomelib", "tcp", "project_version", "3.1.0");
        }
        startEspHomeServer();
#endif

        ensureWebServerStarted();
        onMqttAfterWifi();
#if defined(SYSLOG)
        resetSyslog();
        initSyslog();
#endif
    }
}

static void configureWifiDisconnected() {
    Serial.println("WiFi: connection lost (event)");
    wifiStatus.connectionStatus = ConnState::Disconnected;
    wifiStatus.signalStrengthPercent = 0;
    wifiStatus.rssi = 0;
    const uint32_t now = millis();
    if (s_wifiDisconnectedSinceMs == 0) {
        s_wifiDisconnectedSinceMs = now;
    }
    rssiTimer.detach();
    wifiReconnectTimer.attach(10, wifiReconnectTimerCb);
    if (mdnsStarted) {
        MDNS.end();
    }
    mdnsStarted = false;
#if defined(ESPHOME_API)
    stopEspHomeServer();
#endif
    updateDisplayStatus();
}

static void handleWifiDisconnected() {
    if (wifiStatus.connectionStatus == ConnState::Connected || s_wifiDisconnectedSinceMs == 0) {
        configureWifiDisconnected();
    }
}

static void applyAdvancedWiFiSettings() {
    wifi_config_t config;
    if (esp_wifi_get_config(WIFI_IF_STA, &config) == ESP_OK) {
        config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
#ifdef REQUIRE_MINIMUM_WPA2_PSK
        // This is necessary to prevent the device from Evil Twin attacks, where an attacker creates an additional network with the same
        // SSID as the one selected. WPA2_PSK will detect that and even prevent sending the password.

        // Enable minimal WPA2_PSK level (also allows WPA3 or other more secure modes)
        config.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;
#endif // REQUIRE_MINIMUM_WPA2_PSK
        esp_wifi_set_config(WIFI_IF_STA, &config);
    }
}


static String readNetworkString(const char *key, const char *fallback = "") {
    std::string value;
    if (nvs_read_string(key, value)) {
        return String(value.c_str());
    }
    return String(fallback);
}

static bool parseIPAddressValue(const String &value, IPAddress &address) {
    return value.length() > 0 && address.fromString(value);
}

static String getSavedHostname() {
    String hostname = readNetworkString(NVS_KEY_NET_HOST, "Omni-IO");
    hostname.trim();
    return hostname.length() > 0 ? hostname : String("Omni-IO");
}

static void applySavedNetworkSettings() {
    const String hostname = getSavedHostname();
    WiFi.setHostname(hostname.c_str());

    bool dhcp = true;
    nvs_read_bool(NVS_KEY_NET_DHCP, dhcp);
    if (dhcp) {
        return;
    }

    IPAddress ip, gateway, mask, dns1, dns2;
    const bool hasIp = parseIPAddressValue(readNetworkString(NVS_KEY_NET_IP), ip);
    const bool hasGateway = parseIPAddressValue(readNetworkString(NVS_KEY_NET_GW), gateway);
    const bool hasMask = parseIPAddressValue(readNetworkString(NVS_KEY_NET_MASK), mask);
    parseIPAddressValue(readNetworkString(NVS_KEY_NET_DNS1), dns1);
    parseIPAddressValue(readNetworkString(NVS_KEY_NET_DNS2), dns2);

    if (hasIp && hasGateway && hasMask) {
        if (!WiFi.config(ip, gateway, mask, dns1, dns2)) {
            Serial.println("WiFi: static network config failed, falling back to DHCP");
        }
    } else {
        Serial.println("WiFi: static network config incomplete, falling back to DHCP");
    }
}
static std::string getConfiguredSSID() {
    wifi_config_t conf {};
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK) {
        return {};
    }

    return std::string(reinterpret_cast<const char*>(conf.sta.ssid));
}

static void triggerWiFiReconnect() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi: Trigger WiFi reconnect...");
        applyAdvancedWiFiSettings();
        WiFi.mode(WIFI_STA);
        WiFi.begin();
        s_lastWiFiReconnectAttemptMs = millis();
        s_wifiReconnectAttempts++;
    }
}

static void resetWiFiStack() {
    Serial.println("WiFi: Resetting WiFi stack after prolonged disconnect...");
    addLogMessage("WiFi: resetting WiFi stack");
    WiFi.disconnect(false, false);
    vTaskDelay(pdMS_TO_TICKS(250));
    WiFi.mode(WIFI_OFF);
    vTaskDelay(pdMS_TO_TICKS(500));
    WiFi.mode(WIFI_STA);
    applySavedNetworkSettings();
    applyAdvancedWiFiSettings();
    WiFi.begin();
    s_lastWiFiStackResetMs = millis();
    s_lastWiFiReconnectAttemptMs = s_lastWiFiStackResetMs;
}

static bool readFallbackEnabled();
static uint16_t readFallbackU16(const char *key, uint16_t fallback);
static uint32_t readFallbackTimeoutMs();
static void runConfigPortal(const std::string& ssid, bool hasWifiConfiguration);

static void maintainWiFiConnection() {
    const uint32_t now = millis();

    if (WiFi.status() == WL_CONNECTED) {
        if (wifiStatus.connectionStatus != ConnState::Connected) {
            handleWifiConnected();
        } else {
            wifiStatus.rssi = WiFi.RSSI();
            wifiStatus.signalStrengthPercent = rssiToQuality(wifiStatus.rssi);
        }
        return;
    }

    handleWifiDisconnected();

    if (s_wifiDisconnectedSinceMs == 0) {
        s_wifiDisconnectedSinceMs = now;
    }

    if (static_cast<int32_t>(now - s_lastWiFiReconnectAttemptMs) >= static_cast<int32_t>(WIFI_RECONNECT_INTERVAL_MS)) {
        triggerWiFiReconnect();
    }

    const uint16_t runtimeFallbackRetries = readFallbackU16(NVS_KEY_FB_RUN, 0);
    if (readFallbackEnabled() && runtimeFallbackRetries > 0 &&
        s_wifiReconnectAttempts >= runtimeFallbackRetries) {
        Serial.println("WiFi: opening fallback AP after reconnect retries");
        addLogMessage("WiFi: opening fallback AP after reconnect retries");
        s_wifiReconnectAttempts = 0;
        runConfigPortal(getConfiguredSSID(), true);
    }

    if (static_cast<int32_t>(now - s_wifiDisconnectedSinceMs) >= static_cast<int32_t>(WIFI_STACK_RESET_AFTER_MS) &&
        static_cast<int32_t>(now - s_lastWiFiStackResetMs) >= static_cast<int32_t>(WIFI_STACK_RESET_AFTER_MS)) {
        resetWiFiStack();
    }

    if (static_cast<int32_t>(now - s_wifiDisconnectedSinceMs) >= static_cast<int32_t>(WIFI_RESTART_AFTER_MS)) {
        Serial.println("WiFi: disconnected too long, restarting device");
        addLogMessage("WiFi: disconnected too long, restarting device");
        esp_restart();
    }
}


static bool readFallbackEnabled() {
    bool enabled = true;
    nvs_read_bool(NVS_KEY_FB_ENABLED, enabled);
    return enabled;
}

static uint16_t readFallbackU16(const char *key, uint16_t fallback) {
    uint16_t value = fallback;
    nvs_read_u16(key, value);
    return value;
}

static uint32_t readFallbackTimeoutMs() {
    const uint16_t timeoutSeconds = readFallbackU16(NVS_KEY_FB_TIMEOUT, 600);
    return static_cast<uint32_t>(timeoutSeconds) * 1000UL;
}
static std::tuple<int, int> millisToMinutesAndSeconds(long millis) {
    auto secondsRemaining = millis / 1000;
    return { secondsRemaining / 60, secondsRemaining % 60 };
}

static void runConfigPortal(const std::string& ssid, bool hasWifiConfiguration) {
    if (hasWifiConfiguration) {
        Serial.println("WiFi: Configured network not found, opening Config Portal...");
    } else {
        Serial.println("WiFi: No WiFi network configured, opening Config Portal...");
    }

    WiFiManager wm;

    applyAdvancedWiFiSettings();
    wm.setConfigPortalBlocking(false);
    wm.setDisableConfigPortal(true); // allow config portal shutdown when previous configured wifi comes available.
    const uint32_t portalTimeoutMs = readFallbackTimeoutMs();
    if (portalTimeoutMs > 0) {
        wm.setConfigPortalTimeout(portalTimeoutMs / 1000);
    }
    wm.autoConnect("iohc-setup");

    const unsigned long portalStartTime = millis();
    bool portalClosed = false;
    while (!portalClosed) {
        // Keep telling this info to keep it visible on the display
        if (hasWifiConfiguration) {
            displayCustomMessage("WiFi not found.", ssid.c_str());
        } else {
            displayCustomMessage("WiFi not configured.");
        }
        displayCustomMessage("Custom WiFi AP", "iohc-setup");

        const long millisRemaining = portalTimeoutMs == 0 ? 0 : static_cast<long>(portalTimeoutMs) - static_cast<long>(millis() - portalStartTime);
        if (portalTimeoutMs > 0) {
            auto remainingTime = millisToMinutesAndSeconds(millisRemaining);
            char timeBuf[32];
            snprintf(timeBuf, sizeof(timeBuf), "%2dm %02ds", std::get<0>(remainingTime), std::get<1>(remainingTime));
            displayCustomMessage("Remaining time", timeBuf);
        } else {
            displayCustomMessage("Remaining time", "disabled");
        }

        const bool connected = wm.process(); // Required for async config portal handling

        vTaskDelay(pdMS_TO_TICKS(100));

        if (connected || WiFi.status() == WL_CONNECTED) {
            portalClosed = true;

            if (connected) {
                // workaround for bug in WiFiManager that causes the config portal webserver not to be shut down correctly (keeps port in use)
                esp_restart();
            }
        } else if (portalTimeoutMs > 0 && millisRemaining < 0) {
            Serial.println("WiFi: Config portal timeout, closing portal...");
            portalClosed = true;

            if (hasWifiConfiguration) {
                Serial.printf("WiFi: Device keeps waiting for connection on network: %s. Restart to re-open config portal.\n", ssid.c_str());
            } else {
                Serial.println("WiFi: Restart the device manually to re-open the config portal!");
            }
        }
    }

    clearDisplayMessages();
}

static void wifiWorker(void * pvParameters) {
    wl_status_t status = WL_DISCONNECTED;

    WiFi.mode(WIFI_STA);

    const std::string ssid = getConfiguredSSID();
    const bool hasWifiConfiguration = !ssid.empty();
    if (hasWifiConfiguration) {
        const uint16_t bootRetries = readFallbackU16(NVS_KEY_FB_BOOT, 3);
        for (uint16_t attempt = 0; attempt < bootRetries && status != WL_CONNECTED; ++attempt) {
            applyAdvancedWiFiSettings();
            WiFi.begin();
            Serial.printf("WiFi: Attempt connection to '%s' (%u/%u)...\n", ssid.c_str(), attempt + 1, bootRetries);
            status = (wl_status_t)WiFi.waitForConnectResult(10000);
        }
    }
    if (status != WL_CONNECTED) {
        if (!hasWifiConfiguration || readFallbackEnabled()) {
            runConfigPortal(ssid, hasWifiConfiguration);
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        // If the initial connection succeeds before the event callback is
        // registered or processed, bootstrap the connected state here.
        handleWifiConnected();
    } else {
        configureWifiDisconnected();
    }

    uint32_t events = 0;
    while (true) {
        events = 0;
        xTaskNotifyWait(0, UINT32_MAX, &events, pdMS_TO_TICKS(5000));

        if ((events & WIFI_NOTIFY_DISCONNECTED) != 0) {
            handleWifiDisconnected();
        }

        if ((events & WIFI_NOTIFY_RECONNECT) != 0) {
            triggerWiFiReconnect();
        }

        if ((events & WIFI_NOTIFY_GOT_IP) != 0 &&
            wifiStatus.connectionStatus != ConnState::Connected) {
            handleWifiConnected();
        }

        maintainWiFiConnection();
    }
}

static void onWiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            notifyWiFiWorker(WIFI_NOTIFY_GOT_IP);
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            notifyWiFiWorker(WIFI_NOTIFY_DISCONNECTED);
            break;

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void initWifi() {
    // Set hostname before the WiFi stack initialises so the DHCP client
    // advertises the correct name from the very first connection attempt,
    // including after auto-reconnects where the connect task never runs.
    applySavedNetworkSettings();

    WiFi.onEvent(onWiFiEvent);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);

    xTaskCreatePinnedToCore(wifiWorker, "WiFi_Worker", 8192, NULL, 3, &wifiWorkerTaskHandle, 1);

    Serial.printf("WiFi MAC: %s\n", WiFi.macAddress().c_str());
}

String getConfiguredWiFiSSID() {
    return String(getConfiguredSSID().c_str());
}

void saveWiFiCredentials(const String &ssid, const String &password) {
    String passwordToSave = password;
    if (passwordToSave.length() == 0) {
        wifi_config_t conf {};
        if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
            passwordToSave = String(reinterpret_cast<const char*>(conf.sta.password));
        }
    }

    WiFi.persistent(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), passwordToSave.c_str());
}

void clearWifi() {
    WiFiManager wm;
    wm.resetSettings();
    WiFi.disconnect(true, true);
    esp_restart();
}

