#ifndef SUN_HELPER_H
#define SUN_HELPER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <map>
#include <string>

enum class SunCondition {
    Disabled = 0,
    Night = 1,
    OutsideFacade = 2,
    Cloudy = 3,
    Sunny = 4,
    WindAlert = 5,
    ManualHold = 6
};

struct SunMetrics {
    float elevation = 0.0f;       // Sun altitude angle in degrees (-90 to +90)
    float azimuth = 0.0f;         // Sun compass direction in degrees (0 to 360, 180=South)
    float directRadiation = 0.0f; // W/m2 from Open-Meteo
    float cloudCover = 0.0f;      // 0 to 100%
    float temperature = 0.0f;     // Celsius
    float windSpeed = 0.0f;       // km/h
    bool isDay = false;
    uint32_t lastUpdateEpoch = 0;
};

struct SunConfig {
    bool enabled = false;
    float latitude = 52.3676f;         // Default: Amsterdam
    float longitude = 4.9041f;
    float azimuthStart = 120.0f;       // South-East
    float azimuthEnd = 260.0f;         // West
    float minElevation = 10.0f;        // Minimum sun altitude
    float radiationThreshold = 200.0f; // W/m2 to consider sunny
    float maxCloudCover = 75.0f;       // % cloud cover limit
    uint16_t sunOnDelayMin = 5;        // Debounce before closing
    uint16_t sunOffDelayMin = 15;      // Debounce before opening
    float maxWindSpeed = 35.0f;        // Wind safety limit km/h (0 = disabled)
    std::string windAction = "open";   // "open" (omhoog), "close" (omlaag), "none" (geen actie)
    bool nightAutoOpen = true;         // Auto retract screens at sunset
    std::map<std::string, bool> enabledScreens; // Screen description -> auto enabled
};

class SunHelper {
public:
    static SunHelper *getInstance();

    void begin();
    void tick();
    void evaluateNow();

    const SunConfig &getConfig() const { return config; }
    void setConfig(const SunConfig &newConfig);
    bool loadConfig();
    bool saveConfig();

    const SunMetrics &getMetrics() const { return metrics; }
    SunCondition getCurrentCondition() const { return condition; }
    const char *getConditionString() const;
    bool isActionActive() const { return actionActive; }
    uint32_t getSunnyElapsedSeconds() const;
    uint32_t getCloudyElapsedSeconds() const;
    uint32_t getNextActionCountdownSeconds() const;

    // Astronomical position calculator
    static void calculateSolarPosition(float lat, float lon, time_t epoch, float &elevation, float &azimuth);

private:
    SunHelper();

    SunConfig config;
    SunMetrics metrics;
    SunCondition condition = SunCondition::Disabled;
    bool actionActive = false; // true if screens are currently closed due to sun

    uint32_t lastPollMillis = 0;
    uint32_t sunnyStartMillis = 0;
    uint32_t notSunnyStartMillis = 0;
    uint32_t lastActionMillis = 0;
    bool hasSunnyStart = false;
    bool hasNotSunnyStart = false;

    void fetchWeatherApi();
    void evaluateState();
    void triggerSunAction(bool closeScreens);
};

#endif // SUN_HELPER_H
