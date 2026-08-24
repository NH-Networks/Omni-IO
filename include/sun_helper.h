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
    RainAlert = 6,
    SafetyLockout = 7,
    ColdHold = 8,
    HotDayPreCool = 9,
    ManualHold = 10
};

struct SunMetrics {
    float elevation = 0.0f;          // Sun altitude angle in degrees (-90 to +90)
    float azimuth = 0.0f;            // Sun compass direction in degrees (0 to 360, 180=South)
    float directRadiation = 0.0f;    // W/m2 from Open-Meteo
    float effectiveRadiation = 0.0f; // W/m2 adjusted for solar incidence angle on facade
    float cloudCover = 0.0f;         // 0 to 100%
    float temperature = 0.0f;        // Celsius
    float windSpeed = 0.0f;          // km/h (sustained)
    float windGusts = 0.0f;          // km/h (peak gusts)
    float windDirection = 0.0f;      // degrees (0-360)
    float precipitation = 0.0f;      // mm/h
    int weatherCode = 0;             // WMO weather code (0=clear, 51-67=rain, 80-82=showers, 95-99=thunderstorm)
    float forecastMaxTemp = 0.0f;    // Today's forecast max temperature in Celsius
    int forecastPrecipProb = 0;      // Today's max precipitation probability (%)
    bool isDay = false;
    uint32_t lastUpdateEpoch = 0;
    uint32_t lockoutRemainingSec = 0; // Seconds remaining in safety lockout
    std::string lockoutReason = "";   // "wind" or "rain"
};

struct SunConfig {
    bool enabled = false;
    float latitude = 52.3676f;         // Default: Amsterdam
    float longitude = 4.9041f;

    // Facade Angle & Solar Window
    float azimuthStart = 120.0f;       // South-East
    float azimuthEnd = 260.0f;         // West
    float minElevation = 10.0f;        // Minimum sun altitude
    float radiationThreshold = 200.0f; // W/m2 to consider sunny
    float maxCloudCover = 75.0f;       // % cloud cover limit
    bool useIncidenceAngle = false;    // Use cosine projection for effective radiation
    float facadeAzimuth = 180.0f;      // Direction the facade faces (180 = South)

    // Debounce & Anti-Flap Timers
    uint16_t sunOnDelayMin = 5;        // Debounce before closing
    uint16_t sunOffDelayMin = 15;      // Debounce before opening
    uint16_t minHoldDurationMin = 10;  // Minimum hold duration after action before reversing

    // Wind & Storm Safety
    float maxWindSpeed = 35.0f;        // Sustained wind safety limit km/h (0 = disabled)
    float maxWindGust = 45.0f;         // Peak wind gust safety limit km/h (0 = disabled)
    uint16_t windLockoutMin = 30;      // Lockout duration after storm alert (minutes)
    std::string windAction = "open";   // "open" (omhoog/retract), "close" (omlaag), "none"

    // Rain & Precipitation Safety
    bool rainSafetyEnabled = true;     // Retract on rain/showers/snow/thunderstorm
    std::string rainAction = "open";   // "open", "close", "none"
    uint16_t rainLockoutMin = 20;      // Lockout duration after rain alert (minutes)

    // Climate / Temperature Threshold (Passive Solar Heating)
    bool tempFilterEnabled = true;     // Only close screens if temp is above minimum
    float minTemperature = 19.0f;      // Celsius (below this, keep screens open for solar heating)

    // Hot-Day Pre-Cooling Scenario
    bool hotDayForecastEnabled = false;// Deploy early in the morning if hot day is forecast
    float hotDayThresholdTemp = 26.0f; // Forecast max temp to trigger hot-day mode

    // General & Retract
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
    uint32_t getLockoutRemainingSeconds() const;

    // Astronomical position & incidence calculators
    static void calculateSolarPosition(float lat, float lon, time_t epoch, float &elevation, float &azimuth);
    static float calculateEffectiveRadiation(float elevationDeg, float azimuthDeg, float facadeAzimuthDeg, float directRadiation);

private:
    SunHelper();

    SunConfig config;
    SunMetrics metrics;
    SunCondition condition = SunCondition::Disabled;
    bool actionActive = false; // true if screens are currently closed due to sun / hot-day

    uint32_t lastPollMillis = 0;
    uint32_t sunnyStartMillis = 0;
    uint32_t notSunnyStartMillis = 0;
    uint32_t lastActionMillis = 0;
    bool hasSunnyStart = false;
    bool hasNotSunnyStart = false;

    // Safety lockout tracking
    uint32_t lockoutEndMillis = 0;
    bool isLockedOut = false;
    std::string currentLockoutReason = "";

    void fetchWeatherApi();
    void evaluateState();
    void triggerSunAction(bool closeScreens);
    void enterLockout(uint16_t durationMinutes, const std::string &reason);
    bool checkLockoutActive();
};

#endif // SUN_HELPER_H
