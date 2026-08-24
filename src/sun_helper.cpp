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
    config.useIncidenceAngle = doc["useIncidenceAngle"] | false;
    config.facadeAzimuth = doc["facadeAzimuth"] | 180.0f;

    config.sunOnDelayMin = doc["sunOnDelayMin"] | 5;
    config.sunOffDelayMin = doc["sunOffDelayMin"] | 15;
    config.minHoldDurationMin = doc["minHoldDurationMin"] | 10;

    config.maxWindSpeed = doc["maxWindSpeed"] | 35.0f;
    config.maxWindGust = doc["maxWindGust"] | 45.0f;
    config.windLockoutMin = doc["windLockoutMin"] | 30;
    config.windAction = doc["windAction"] | "open";

    config.rainSafetyEnabled = doc["rainSafetyEnabled"] | true;
    config.rainAction = doc["rainAction"] | "open";
    config.rainLockoutMin = doc["rainLockoutMin"] | 20;

    config.tempFilterEnabled = doc["tempFilterEnabled"] | true;
    config.minTemperature = doc["minTemperature"] | 19.0f;

    config.hotDayForecastEnabled = doc["hotDayForecastEnabled"] | false;
    config.hotDayThresholdTemp = doc["hotDayThresholdTemp"] | 26.0f;

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
    doc["useIncidenceAngle"] = config.useIncidenceAngle;
    doc["facadeAzimuth"] = config.facadeAzimuth;

    doc["sunOnDelayMin"] = config.sunOnDelayMin;
    doc["sunOffDelayMin"] = config.sunOffDelayMin;
    doc["minHoldDurationMin"] = config.minHoldDurationMin;

    doc["maxWindSpeed"] = config.maxWindSpeed;
    doc["maxWindGust"] = config.maxWindGust;
    doc["windLockoutMin"] = config.windLockoutMin;
    doc["windAction"] = config.windAction;

    doc["rainSafetyEnabled"] = config.rainSafetyEnabled;
    doc["rainAction"] = config.rainAction;
    doc["rainLockoutMin"] = config.rainLockoutMin;

    doc["tempFilterEnabled"] = config.tempFilterEnabled;
    doc["minTemperature"] = config.minTemperature;

    doc["hotDayForecastEnabled"] = config.hotDayForecastEnabled;
    doc["hotDayThresholdTemp"] = config.hotDayThresholdTemp;

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
        case SunCondition::RainAlert: return "rain_alert";
        case SunCondition::SafetyLockout: return "safety_lockout";
        case SunCondition::ColdHold: return "cold_hold";
        case SunCondition::HotDayPreCool: return "hot_day_precool";
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
    if ((condition == SunCondition::Sunny || condition == SunCondition::HotDayPreCool) && !actionActive) {
        uint32_t reqSec = (condition == SunCondition::HotDayPreCool) ? 60 : (config.sunOnDelayMin * 60);
        uint32_t elapsed = getSunnyElapsedSeconds();
        return (elapsed >= reqSec) ? 0 : (reqSec - elapsed);
    }
    if (condition != SunCondition::Sunny && condition != SunCondition::HotDayPreCool && actionActive) {
        uint32_t reqSec = config.sunOffDelayMin * 60;
        uint32_t elapsed = getCloudyElapsedSeconds();
        return (elapsed >= reqSec) ? 0 : (reqSec - elapsed);
    }
    return 0;
}

void SunHelper::enterLockout(uint16_t durationMinutes, const std::string &reason) {
    if (durationMinutes == 0) return;
    uint32_t nowMs = millis();
    lockoutEndMillis = nowMs + (uint32_t)durationMinutes * 60000;
    isLockedOut = true;
    currentLockoutReason = reason;
}

bool SunHelper::checkLockoutActive() {
    if (!isLockedOut) return false;
    uint32_t nowMs = millis();
    if ((int32_t)(lockoutEndMillis - nowMs) <= 0) {
        isLockedOut = false;
        currentLockoutReason = "";
        return false;
    }
    return true;
}

uint32_t SunHelper::getLockoutRemainingSeconds() const {
    if (!isLockedOut) return 0;
    uint32_t nowMs = millis();
    if ((int32_t)(lockoutEndMillis - nowMs) <= 0) return 0;
    return (lockoutEndMillis - nowMs) / 1000;
}

/**
 * Calculates effective solar radiation on a vertical window surface
 * based on incidence angle cosine projection.
 */
float SunHelper::calculateEffectiveRadiation(float elevationDeg, float azimuthDeg, float facadeAzimuthDeg, float directRadiation) {
    if (elevationDeg <= 0.0f || directRadiation <= 0.0f) {
        return 0.0f;
    }
    double elevRad = elevationDeg * DEG_TO_RAD_D;
    double diffAzRad = (azimuthDeg - facadeAzimuthDeg) * DEG_TO_RAD_D;
    double cosTheta = cos(elevRad) * cos(diffAzRad);
    if (cosTheta <= 0.0) {
        return 0.0f;
    }
    return (float)(directRadiation * cosTheta);
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

    // Call Open-Meteo public forecast API with current conditions and daily forecast
    String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(config.latitude, 4) +
                 "&longitude=" + String(config.longitude, 4) +
                 "&current=direct_radiation,cloud_cover,temperature_2m,wind_speed_10m,wind_gusts_10m,wind_direction_10m,precipitation,weather_code,is_day" +
                 "&daily=temperature_2m_max,precipitation_probability_max&timezone=auto";

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
        if (!err) {
            if (doc["current"].is<JsonObject>()) {
                JsonObject cur = doc["current"].as<JsonObject>();
                metrics.directRadiation = cur["direct_radiation"] | 0.0f;
                metrics.cloudCover = cur["cloud_cover"] | 0.0f;
                metrics.temperature = cur["temperature_2m"] | 0.0f;
                metrics.windSpeed = cur["wind_speed_10m"] | 0.0f;
                metrics.windGusts = cur["wind_gusts_10m"] | 0.0f;
                metrics.windDirection = cur["wind_direction_10m"] | 0.0f;
                metrics.precipitation = cur["precipitation"] | 0.0f;
                metrics.weatherCode = cur["weather_code"] | 0;
                metrics.isDay = (cur["is_day"] | 0) == 1;
                metrics.lastUpdateEpoch = nowSec;
            }
            if (doc["daily"].is<JsonObject>()) {
                JsonObject daily = doc["daily"].as<JsonObject>();
                if (daily["temperature_2m_max"].is<JsonArray>() && daily["temperature_2m_max"].size() > 0) {
                    metrics.forecastMaxTemp = daily["temperature_2m_max"][0] | 0.0f;
                }
                if (daily["precipitation_probability_max"].is<JsonArray>() && daily["precipitation_probability_max"].size() > 0) {
                    metrics.forecastPrecipProb = daily["precipitation_probability_max"][0] | 0;
                }
            }

            metrics.effectiveRadiation = calculateEffectiveRadiation(metrics.elevation, metrics.azimuth, config.facadeAzimuth, metrics.directRadiation);

            Serial.printf("[SUN] Open-Meteo: Rad=%.1f W/m2 (Eff=%.1f), Cloud=%.1f%%, Wind=%.1f km/h (Gust=%.1f), Temp=%.1f C (Max=%.1f C), Rain=%.1f mm/h, Code=%d\n",
                          metrics.directRadiation, metrics.effectiveRadiation, metrics.cloudCover,
                          metrics.windSpeed, metrics.windGusts, metrics.temperature, metrics.forecastMaxTemp,
                          metrics.precipitation, metrics.weatherCode);
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
    metrics.effectiveRadiation = calculateEffectiveRadiation(metrics.elevation, metrics.azimuth, config.facadeAzimuth, metrics.directRadiation);
    metrics.lockoutRemainingSec = getLockoutRemainingSeconds();
    metrics.lockoutReason = currentLockoutReason;

    // 1. Hard Rain & Weather Alert Check
    if (config.rainSafetyEnabled && (metrics.precipitation >= 0.1f || (metrics.weatherCode >= 51 && metrics.weatherCode <= 99))) {
        condition = SunCondition::RainAlert;
        hasSunnyStart = false;
        hasNotSunnyStart = false;
        enterLockout(config.rainLockoutMin, "rain");
        metrics.lockoutRemainingSec = getLockoutRemainingSeconds();
        metrics.lockoutReason = currentLockoutReason;

        if (config.rainAction == "open" && actionActive) {
            addLogMessage(String("Sun Automation: Rain/showers detected (") + String(metrics.precipitation, 1) + " mm/h, code " + String(metrics.weatherCode) + ") -> Retracting screens (protection)");
            triggerSunAction(false);
        } else if (config.rainAction == "close" && !actionActive) {
            addLogMessage(String("Sun Automation: Rain/showers detected -> Closing shutters"));
            triggerSunAction(true);
        }
        return;
    }

    // 2. Hard Wind & Gusts Safety Check
    bool windAlert = (config.maxWindSpeed > 0 && metrics.windSpeed >= config.maxWindSpeed) ||
                     (config.maxWindGust > 0 && metrics.windGusts >= config.maxWindGust);

    if (windAlert) {
        condition = SunCondition::WindAlert;
        hasSunnyStart = false;
        hasNotSunnyStart = false;
        enterLockout(config.windLockoutMin, "wind");
        metrics.lockoutRemainingSec = getLockoutRemainingSeconds();
        metrics.lockoutReason = currentLockoutReason;

        if (config.windAction == "close") {
            if (!actionActive) {
                addLogMessage(String("Sun Automation: Wind speed alert (Speed=") + String(metrics.windSpeed, 1) + " km/h, Gusts=" + String(metrics.windGusts, 1) + " km/h) -> Closing shutters (storm protection)");
                triggerSunAction(true);
            }
        } else if (config.windAction == "open") {
            if (actionActive) {
                addLogMessage(String("Sun Automation: Wind speed alert (Speed=") + String(metrics.windSpeed, 1) + " km/h, Gusts=" + String(metrics.windGusts, 1) + " km/h) -> Retracting screens (safety)");
                triggerSunAction(false);
            }
        }
        return;
    }

    // 3. Safety Lockout Check (Post-storm/rain hold)
    if (checkLockoutActive()) {
        condition = SunCondition::SafetyLockout;
        hasSunnyStart = false;
        hasNotSunnyStart = false;
        return;
    }

    // 4. Night Check
    if (!metrics.isDay || metrics.elevation <= 0) {
        condition = SunCondition::Night;
        hasSunnyStart = false;
        if (config.nightAutoOpen && actionActive) {
            addLogMessage("Sun Automation: Sunset/Night detected -> Opening screens");
            triggerSunAction(false);
        }
        return;
    }

    // 5. Facade Angle / Solar Window Check
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
    } else {
        float evaluatedRadiation = config.useIncidenceAngle ? metrics.effectiveRadiation : metrics.directRadiation;
        if (evaluatedRadiation < config.radiationThreshold || metrics.cloudCover > config.maxCloudCover) {
            condition = SunCondition::Cloudy;
        } else {
            // Check cold temperature hold (Passive solar heating in cold season)
            if (config.tempFilterEnabled && metrics.temperature < config.minTemperature) {
                condition = SunCondition::ColdHold;
            } else {
                condition = SunCondition::Sunny;
            }
        }
    }

    // 6. Hot-Day Pre-Cooling Scenario Check
    if (config.hotDayForecastEnabled && metrics.forecastMaxTemp >= config.hotDayThresholdTemp) {
        // If today is predicted to be hot, deploy early once sun enters facade elevation/azimuth
        if (inAzimuthRange && inElevationRange && metrics.temperature >= (config.minTemperature - 2.0f)) {
            if (condition == SunCondition::Cloudy || condition == SunCondition::Sunny) {
                condition = SunCondition::HotDayPreCool;
            }
        }
    }

    // 7. Hysteresis, Debounce & Anti-Flap Logic
    uint32_t nowMs = millis();
    bool wantsDeploy = (condition == SunCondition::Sunny || condition == SunCondition::HotDayPreCool);

    // Anti-flap: enforce minimum duration between state changes
    uint32_t minHoldSec = config.minHoldDurationMin * 60;
    bool holdActive = (lastActionMillis != 0 && (nowMs - lastActionMillis) / 1000 < minHoldSec);

    if (wantsDeploy) {
        hasNotSunnyStart = false;
        if (!hasSunnyStart) {
            hasSunnyStart = true;
            sunnyStartMillis = nowMs;
        }

        uint32_t sunnyElapsedSec = (nowMs - sunnyStartMillis) / 1000;
        uint32_t reqSec = (condition == SunCondition::HotDayPreCool) ? 60 : (config.sunOnDelayMin * 60);

        if (sunnyElapsedSec >= reqSec && !actionActive) {
            if (!holdActive) {
                const char *reason = (condition == SunCondition::HotDayPreCool) ? "Hot-Day Pre-Cooling forecast" : "Sun active";
                addLogMessage(String("Sun Automation: ") + reason + " -> Closing screens");
                triggerSunAction(true);
            }
        }
    } else {
        // Condition is Cloudy, OutsideFacade, ColdHold, or Night
        hasSunnyStart = false;
        if (!hasNotSunnyStart) {
            hasNotSunnyStart = true;
            notSunnyStartMillis = nowMs;
        }

        uint32_t notSunnyElapsedSec = (nowMs - notSunnyStartMillis) / 1000;
        uint32_t reqSec = config.sunOffDelayMin * 60;

        if (notSunnyElapsedSec >= reqSec && actionActive) {
            if (!holdActive) {
                const char *reason = (condition == SunCondition::ColdHold) ? "Cold weather hold (allowing solar heat)" : "Sun absent";
                addLogMessage(String("Sun Automation: ") + reason + " -> Opening screens");
                triggerSunAction(false);
            }
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
        // Check debounce and lockout status every 15 seconds
        static uint32_t lastEvalMs = 0;
        if (nowMs - lastEvalMs >= 15000) {
            lastEvalMs = nowMs;
            if (config.enabled) {
                evaluateState();
            }
        }
    }
}
