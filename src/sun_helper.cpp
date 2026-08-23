#include <sun_helper.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <iohcRemote1W.h>
#include <log_buffer.h>
#include <cmath>
#include <ctime>

static SunHelper *s_instance = nullptr;
static constexpr const char *SUN_CONFIG_PATH = "/sun_config.json";
static constexpr uint32_t POLL_INTERVAL_MS = 600000; // 10 minutes
static constexpr double DEG_TO_RAD_D = 0.017453292519943295769236907684886;
static constexpr double RAD_TO_DEG_D = 57.295779513082320876798154814105;

SunHelper *SunHelper::getInstance() {
    if (!s_instance) {
        s_instance = new SunHelper();
    }
    return s_instance;
}

SunHelper::SunHelper() {
}

void SunHelper::begin() {
    loadConfig();
    Serial.println("[SUN] SunHelper initialized. Enabled: " + String(config.enabled ? "true" : "false"));
    if (config.enabled) {
        evaluateNow();
    }
}

void SunHelper::setConfig(const SunConfig &newConfig) {
    config = newConfig;
    saveConfig();
    evaluateNow();
}

bool SunHelper::loadConfig() {
    if (!LittleFS.exists(SUN_CONFIG_PATH)) {
        return false;
    }
    File f = LittleFS.open(SUN_CONFIG_PATH, "r");
    if (!f) {
        return false;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        Serial.printf("[SUN] Failed to parse %s: %s\n", SUN_CONFIG_PATH, err.c_str());
        return false;
    }

    config.enabled = doc["enabled"] | false;
    config.latitude = doc["latitude"] | 52.3676f;
    config.longitude = doc["longitude"] | 4.9041f;
    config.azimuthStart = doc["azimuthStart"] | 120.0f;
    config.azimuthEnd = doc["azimuthEnd"] | 260.0f;
    config.minElevation = doc["minElevation"] | 10.0f;
    config.radiationThreshold = doc["radiationThreshold"] | 200.0f;
    config.maxCloudCover = doc["maxCloudCover"] | 75.0f;
    config.sunOnDelayMin = doc["sunOnDelayMin"] | 5;
    config.sunOffDelayMin = doc["sunOffDelayMin"] | 15;
    config.maxWindSpeed = doc["maxWindSpeed"] | 35.0f;
    config.nightAutoOpen = doc["nightAutoOpen"] | true;

    config.enabledScreens.clear();
    if (doc["screens"].is<JsonObject>()) {
        JsonObject screens = doc["screens"].as<JsonObject>();
        for (JsonPair kv : screens) {
            config.enabledScreens[kv.key().c_str()] = kv.value().as<bool>();
        }
    }
    return true;
}

bool SunHelper::saveConfig() {
    JsonDocument doc;
    doc["enabled"] = config.enabled;
    doc["latitude"] = config.latitude;
    doc["longitude"] = config.longitude;
    doc["azimuthStart"] = config.azimuthStart;
    doc["azimuthEnd"] = config.azimuthEnd;
    doc["minElevation"] = config.minElevation;
    doc["radiationThreshold"] = config.radiationThreshold;
    doc["maxCloudCover"] = config.maxCloudCover;
    doc["sunOnDelayMin"] = config.sunOnDelayMin;
    doc["sunOffDelayMin"] = config.sunOffDelayMin;
    doc["maxWindSpeed"] = config.maxWindSpeed;
    doc["nightAutoOpen"] = config.nightAutoOpen;

    JsonObject screens = doc["screens"].to<JsonObject>();
    for (const auto &kv : config.enabledScreens) {
        screens[kv.first] = kv.second;
    }

    File f = LittleFS.open(SUN_CONFIG_PATH, "w");
    if (!f) {
        return false;
    }
    serializeJson(doc, f);
    f.close();
    return true;
}

const char *SunHelper::getConditionString() const {
    switch (condition) {
        case SunCondition::Disabled: return "disabled";
        case SunCondition::Night: return "night";
        case SunCondition::OutsideFacade: return "outside_facade";
        case SunCondition::Cloudy: return "cloudy";
        case SunCondition::Sunny: return "sunny";
        case SunCondition::WindAlert: return "wind_alert";
        case SunCondition::ManualHold: return "manual_hold";
        default: return "unknown";
    }
}

uint32_t SunHelper::getSunnyElapsedSeconds() const {
    if (!hasSunnyStart) return 0;
    return (millis() - sunnyStartMillis) / 1000;
}

uint32_t SunHelper::getCloudyElapsedSeconds() const {
    if (!hasNotSunnyStart) return 0;
    return (millis() - notSunnyStartMillis) / 1000;
}

uint32_t SunHelper::getNextActionCountdownSeconds() const {
    if (!config.enabled) return 0;
    if (condition == SunCondition::Sunny && !actionActive) {
        uint32_t reqSec = config.sunOnDelayMin * 60;
        uint32_t elapsed = getSunnyElapsedSeconds();
        return (elapsed >= reqSec) ? 0 : (reqSec - elapsed);
    }
    if (condition != SunCondition::Sunny && actionActive) {
        uint32_t reqSec = config.sunOffDelayMin * 60;
        uint32_t elapsed = getCloudyElapsedSeconds();
        return (elapsed >= reqSec) ? 0 : (reqSec - elapsed);
    }
    return 0;
}

/**
 * Astronomical Solar Position Calculation (NOAA / PSA algorithm)
 * Calculates Sun Elevation (-90 to +90) and Azimuth (0 to 360, 180=South)
 */
void SunHelper::calculateSolarPosition(float lat, float lon, time_t epoch, float &elevation, float &azimuth) {
    if (epoch <= 100000) {
        elevation = 0;
        azimuth = 180;
        return;
    }

    // Julian Date calculation
    double d = (double)epoch / 86400.0 + 2440587.5 - 2451545.0;

    // Mean anomaly and longitude
    double g = fmod(357.529 + 0.98560028 * d, 360.0);
    if (g < 0) g += 360.0;
    double q = fmod(280.459 + 0.98564736 * d, 360.0);
    if (q < 0) q += 360.0;

    double L = q + 1.915 * sin(g * DEG_TO_RAD_D) + 0.020 * sin(2 * g * DEG_TO_RAD_D);
    double e = 23.439 - 0.00000036 * d;

    // Right ascension & Declination
    double sinL = sin(L * DEG_TO_RAD_D);
    double cosL = cos(L * DEG_TO_RAD_D);
    double sinE = sin(e * DEG_TO_RAD_D);
    double cosE = cos(e * DEG_TO_RAD_D);

    double RA = atan2(cosE * sinL, cosL) * RAD_TO_DEG_D;
    if (RA < 0) RA += 360.0;
    double Dec = asin(sinE * sinL) * RAD_TO_DEG_D;

    // Greenwich Mean Sidereal Time
    double GMST = fmod(280.46061837 + 360.98564736629 * d, 360.0);
    if (GMST < 0) GMST += 360.0;
    double LMST = GMST + lon;
    double HA = LMST - RA;
    if (HA < -180.0) HA += 360.0;
    if (HA > 180.0) HA -= 360.0;

    // Solar elevation (Altitude)
    double latRad = lat * DEG_TO_RAD_D;
    double decRad = Dec * DEG_TO_RAD_D;
    double haRad = HA * DEG_TO_RAD_D;

    double sinAlt = sin(latRad) * sin(decRad) + cos(latRad) * cos(decRad) * cos(haRad);
    double altRad = asin(sinAlt);
    elevation = (float)(altRad * RAD_TO_DEG_D);

    // Solar azimuth
    double cosAlt = cos(altRad);
    if (cosAlt > 0.0001) {
        double cosAz = (sin(decRad) - sin(latRad) * sinAlt) / (cos(latRad) * cosAlt);
        if (cosAz > 1.0) cosAz = 1.0;
        if (cosAz < -1.0) cosAz = -1.0;
        double azRad = acos(cosAz);
        double azDeg = azRad * RAD_TO_DEG_D;
        if (sin(haRad) > 0) {
            azimuth = (float)(360.0 - azDeg);
        } else {
            azimuth = (float)azDeg;
        }
    } else {
        azimuth = 180.0f;
    }
}

void SunHelper::evaluateNow() {
    fetchWeatherApi();
    evaluateState();
}

void SunHelper::fetchWeatherApi() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    time_t nowSec = time(nullptr);
    calculateSolarPosition(config.latitude, config.longitude, nowSec, metrics.elevation, metrics.azimuth);

    // Call Open-Meteo public forecast API
    String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(config.latitude, 4) +
                 "&longitude=" + String(config.longitude, 4) +
                 "&current=direct_radiation,cloud_cover,temperature_2m,wind_speed_10m,is_day&timezone=auto";

    HTTPClient http;
    http.setTimeout(7000);
    if (!http.begin(url)) {
        Serial.println("[SUN] Failed to begin HTTP connection to Open-Meteo");
        return;
    }

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err && doc["current"].is<JsonObject>()) {
            JsonObject cur = doc["current"].as<JsonObject>();
            metrics.directRadiation = cur["direct_radiation"] | 0.0f;
            metrics.cloudCover = cur["cloud_cover"] | 0.0f;
            metrics.temperature = cur["temperature_2m"] | 0.0f;
            metrics.windSpeed = cur["wind_speed_10m"] | 0.0f;
            metrics.isDay = (cur["is_day"] | 0) == 1;
            metrics.lastUpdateEpoch = nowSec;

            Serial.printf("[SUN] Open-Meteo: Rad=%.1f W/m2, Cloud=%.1f%%, Wind=%.1f km/h, Temp=%.1f C, Elev=%.1f deg, Azim=%.1f deg\n",
                          metrics.directRadiation, metrics.cloudCover, metrics.windSpeed,
                          metrics.temperature, metrics.elevation, metrics.azimuth);
        } else {
            Serial.println("[SUN] Failed to parse Open-Meteo JSON payload");
        }
    } else {
        Serial.printf("[SUN] Open-Meteo HTTP error: %d\n", httpCode);
    }
    http.end();
}

void SunHelper::evaluateState() {
    if (!config.enabled) {
        condition = SunCondition::Disabled;
        hasSunnyStart = false;
        hasNotSunnyStart = false;
        return;
    }

    time_t nowSec = time(nullptr);
    calculateSolarPosition(config.latitude, config.longitude, nowSec, metrics.elevation, metrics.azimuth);

    // 1. Wind Safety Check
    if (config.maxWindSpeed > 0 && metrics.windSpeed >= config.maxWindSpeed) {
        condition = SunCondition::WindAlert;
        hasSunnyStart = false;
        if (actionActive) {
            addLogMessage(String("Sun Automation: Wind speed alert (") + String(metrics.windSpeed, 1) + " km/h) -> Retracting screens");
            triggerSunAction(false); // Open screens for safety
        }
        return;
    }

    // 2. Night Check
    if (!metrics.isDay || metrics.elevation <= 0) {
        condition = SunCondition::Night;
        hasSunnyStart = false;
        if (config.nightAutoOpen && actionActive) {
            addLogMessage("Sun Automation: Sunset/Night detected -> Opening screens");
            triggerSunAction(false);
        }
        return;
    }

    // 3. Facade Angle / Solar Window Check
    bool inAzimuthRange = false;
    if (config.azimuthStart <= config.azimuthEnd) {
        inAzimuthRange = (metrics.azimuth >= config.azimuthStart && metrics.azimuth <= config.azimuthEnd);
    } else {
        // Range wraps around North (360)
        inAzimuthRange = (metrics.azimuth >= config.azimuthStart || metrics.azimuth <= config.azimuthEnd);
    }

    bool inElevationRange = (metrics.elevation >= config.minElevation);

    if (!inAzimuthRange || !inElevationRange) {
        condition = SunCondition::OutsideFacade;
    } else if (metrics.directRadiation < config.radiationThreshold || metrics.cloudCover > config.maxCloudCover) {
        condition = SunCondition::Cloudy;
    } else {
        condition = SunCondition::Sunny;
    }

    // 4. Hysteresis & Debounce Logic
    uint32_t nowMs = millis();

    if (condition == SunCondition::Sunny) {
        hasNotSunnyStart = false;
        if (!hasSunnyStart) {
            hasSunnyStart = true;
            sunnyStartMillis = nowMs;
        }

        uint32_t sunnyElapsedSec = (nowMs - sunnyStartMillis) / 1000;
        uint32_t reqSec = config.sunOnDelayMin * 60;

        if (sunnyElapsedSec >= reqSec && !actionActive) {
            addLogMessage(String("Sun Automation: Sun active for ") + String(config.sunOnDelayMin) + " min -> Closing screens");
            triggerSunAction(true); // Close screens
        }
    } else {
        // Condition is Cloudy, OutsideFacade, or Night
        hasSunnyStart = false;
        if (!hasNotSunnyStart) {
            hasNotSunnyStart = true;
            notSunnyStartMillis = nowMs;
        }

        uint32_t notSunnyElapsedSec = (nowMs - notSunnyStartMillis) / 1000;
        uint32_t reqSec = config.sunOffDelayMin * 60;

        if (notSunnyElapsedSec >= reqSec && actionActive) {
            addLogMessage(String("Sun Automation: Sun absent for ") + String(config.sunOffDelayMin) + " min -> Opening screens");
            triggerSunAction(false); // Open screens
        }
    }
}

void SunHelper::triggerSunAction(bool closeScreens) {
    actionActive = closeScreens;
    lastActionMillis = millis();

    const auto &remotes = IOHC::iohcRemote1W::getInstance()->getRemotes();
    for (const auto &r : remotes) {
        // If specific screens are configured, check if this screen is enabled
        if (!config.enabledScreens.empty()) {
            auto it = config.enabledScreens.find(r.description);
            if (it != config.enabledScreens.end() && !it->second) {
                continue; // Screen is unchecked / disabled for sun automation
            }
        }

        Tokens cmdTokens;
        cmdTokens.push_back(closeScreens ? "close" : "open");
        cmdTokens.push_back(r.description);

        Serial.printf("[SUN] Dispatching: %s %s (%s)\n",
                      closeScreens ? "close" : "open", r.description.c_str(), r.name.c_str());

        if (closeScreens) {
            IOHC::iohcRemote1W::getInstance()->cmd(IOHC::RemoteButton::Close, &cmdTokens);
        } else {
            IOHC::iohcRemote1W::getInstance()->cmd(IOHC::RemoteButton::Open, &cmdTokens);
        }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

void SunHelper::tick() {
    uint32_t nowMs = millis();
    if (nowMs - lastPollMillis >= POLL_INTERVAL_MS || lastPollMillis == 0) {
        lastPollMillis = nowMs;
        if (config.enabled) {
            evaluateNow();
        }
    } else {
        // Check hysteresis debounce every 15 seconds
        static uint32_t lastEvalMs = 0;
        if (nowMs - lastEvalMs >= 15000) {
            lastEvalMs = nowMs;
            if (config.enabled) {
                evaluateState();
            }
        }
    }
}
