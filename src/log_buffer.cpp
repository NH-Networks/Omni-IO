#include <vector>
#include <Arduino.h>
#include <log_buffer.h>
#include <user_config.h>

#if defined(WEBSERVER)
#include <web_server_handler.h>
#endif
#if defined(SYSLOG)
#include <syslog_helper.h>
#endif

namespace {
    constexpr size_t MAX_LOG_ENTRIES = 60;
    constexpr size_t MAX_LOG_ENTRY_LEN = 384;

    char logRing[MAX_LOG_ENTRIES][MAX_LOG_ENTRY_LEN] = {};
    size_t logStart = 0;
    size_t logCount = 0;
    RTC_NOINIT_ATTR char crashMarker[96];
    RTC_NOINIT_ATTR uint32_t crashMarkerMagic;
    constexpr uint32_t CRASH_MARKER_MAGIC = 0x4d494f50; // MIOP

    bool shouldBroadcastLog(const String &msg) {
        if (msg.length() == 0) {
            return false;
        }

        if (msg.indexOf("2W") >= 0 ||
            msg.indexOf("1W") >= 0 ||
            msg.indexOf("Radio RX") >= 0 ||
            msg.indexOf("Boot reset reason") >= 0 ||
            msg.indexOf("Last crash marker") >= 0) {
            return true;
        }

        if (msg.startsWith("[D]") || msg.startsWith("[V]")) {
            return false;
        }

        if (msg.indexOf("TX: TX-RX DONE") >= 0 ||
            msg.indexOf("State:") >= 0) {
            return false;
        }

        static unsigned long lastBroadcastMs = 0;
        const unsigned long now = millis();
        if (now - lastBroadcastMs < 50) {
            return false;
        }
        lastBroadcastMs = now;
        return true;
    }
}

void addLogMessage(const String &msg) {
    static bool inLogMessage = false;
    if (inLogMessage) {
        return;
    }
    inLogMessage = true;

    static char lastMessage[MAX_LOG_ENTRY_LEN] = {};
    static unsigned long lastMessageMs = 0;
    const unsigned long now = millis();
    const char *text = msg.c_str();

    if (strncmp(text, lastMessage, MAX_LOG_ENTRY_LEN) == 0 && now - lastMessageMs < 3000) {
        inLogMessage = false;
        return;
    }
    snprintf(lastMessage, sizeof(lastMessage), "%s", text);
    lastMessageMs = now;

    size_t slot = (logStart + logCount) % MAX_LOG_ENTRIES;
    if (logCount == MAX_LOG_ENTRIES) {
        slot = logStart;
        logStart = (logStart + 1) % MAX_LOG_ENTRIES;
    } else {
        logCount++;
    }

    snprintf(logRing[slot], MAX_LOG_ENTRY_LEN, "%s", text);
    const String storedMessage(logRing[slot]);
    Serial.printf("[%10lu ms] %s\n", now, storedMessage.c_str());
#if defined(WEBSERVER)
    if (shouldBroadcastLog(storedMessage)) {
        broadcastLog(storedMessage);
    }
#endif
#if defined(SYSLOG)
    sendSyslog(storedMessage);
#endif
    inLogMessage = false;
}

std::vector<String> getLogMessages() {
    std::vector<String> logs;
    logs.reserve(logCount);
    for (size_t idx = 0; idx < logCount; ++idx) {
        logs.emplace_back(logRing[(logStart + idx) % MAX_LOG_ENTRIES]);
    }
    return logs;
}

void setCrashMarker(const char *marker) {
    if (!marker) {
        return;
    }
    snprintf(crashMarker, sizeof(crashMarker), "%s", marker);
    crashMarkerMagic = CRASH_MARKER_MAGIC;
}

String getCrashMarker() {
    if (crashMarkerMagic != CRASH_MARKER_MAGIC) {
        return "";
    }
    crashMarker[sizeof(crashMarker) - 1] = '\0';
    return String(crashMarker);
}
