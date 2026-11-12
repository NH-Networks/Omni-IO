/*
 * Modifications Copyright 2026 CloudAXS.
 * Original upstream portions remain licensed under Apache-2.0.
 */
#include "syslog_helper.h"
#include <user_config.h> // provides SYSLOG, syslog_server, syslog_port

#if defined(SYSLOG)

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_log.h>
#include <esp_random.h>
#include <nvs_helpers.h>
#include <time.h>

// ===== Config (adjust if you like) =====
#ifndef SYSLOG_FACILITY
#define SYSLOG_FACILITY 16 // local0
#endif

#ifndef SYSLOG_APP
#define SYSLOG_APP "OMNI-IO" // rsyslog will use this as %PROGRAMNAME%
#endif

// Define SYSLOG_RFC5424 to send RFC5424 instead of RFC3164
// #define SYSLOG_RFC5424

namespace {
WiFiUDP syslogUdp;
IPAddress syslogIP;
bool syslogReady = false;
bool configLoaded = false;
uint32_t syslogBackoffUntilMs = 0;
uint32_t lastSyslogSendMs = 0;
static constexpr uint32_t SYSLOG_MIN_SEND_INTERVAL_MS = 250;
static constexpr uint32_t SYSLOG_ERROR_BACKOFF_MS = 60000;
static const char *TAG = "SYSLOG";

inline int pri(int facility, int severity) {
  if (severity < 0)
    severity = 6; // default info
  if (severity > 7)
    severity = 7;
  return facility * 8 + severity;
}

void ensureConfigLoaded() {
  if (configLoaded) {
    return;
  }

  bool enabled = syslog_enabled;
  if (nvs_read_bool(NVS_KEY_SYSLOG_ENABLED, enabled)) {
    syslog_enabled = enabled;
  } else {
    nvs_write_bool(NVS_KEY_SYSLOG_ENABLED, syslog_enabled);
  }

  if (!nvs_read_string(NVS_KEY_SYSLOG_SERVER, syslog_server)) {
    // No server stored yet — pre-fill community server as default
    syslog_server = "syslog.local";
    nvs_write_string(NVS_KEY_SYSLOG_SERVER, syslog_server);
  }

  if (!nvs_read_u16(NVS_KEY_SYSLOG_PORT, syslog_port)) {
    syslog_port = 5144;
    nvs_write_u16(NVS_KEY_SYSLOG_PORT, syslog_port);
  }

  if (!nvs_read_string(NVS_KEY_SYSLOG_TAG, syslog_tag) || syslog_tag.empty()) {
    // Auto-generate a random 8-char hex ID on first boot
    char generated[9];
    snprintf(generated, sizeof(generated), "%08x", esp_random());
    syslog_tag = generated;
    nvs_write_string(NVS_KEY_SYSLOG_TAG, syslog_tag);
  }

  configLoaded = true;
}

#ifndef SYSLOG_RFC5424
// RFC3164 timestamp: "Jan  2 15:04:05" (local time)
String rfc3164Timestamp() {
  time_t now = time(nullptr);
  struct tm tmnow;
  localtime_r(&now, &tmnow);
  char buf[32];
  strftime(buf, sizeof(buf), "%b %e %T", &tmnow);
  return String(buf);
}
#else
// RFC5424 timestamp: "YYYY-MM-DDTHH:MM:SSZ" (UTC)
String iso8601UTC() {
  time_t now = time(nullptr);
  struct tm tmnow;
  gmtime_r(&now, &tmnow);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmnow);
  return String(buf);
}
#endif

String currentHostIdent() {
  const char *h = WiFi.getHostname();
  if (h && *h)
    return String(h);
  return WiFi.localIP().toString();
}
} // namespace

void loadSyslogConfig() {
  ensureConfigLoaded();
}

// Initialize UDP + resolve syslog IP from user_config.h
void initSyslog() {
  ensureConfigLoaded();

  if (!syslog_enabled || syslog_server.empty() || syslog_port == 0 || syslog_port > 65535) {
    resetSyslog();
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    resetSyslog();
    return;
  }

  const uint32_t nowMs = millis();
  if (static_cast<int32_t>(nowMs - syslogBackoffUntilMs) < 0) {
    return;
  }

  if (!syslogIP.fromString(syslog_server.c_str())) {
    if (WiFi.hostByName(syslog_server.c_str(), syslogIP) != 1) {
      resetSyslog();
      syslogBackoffUntilMs = nowMs + SYSLOG_ERROR_BACKOFF_MS;
      return;
    }
  }

  if (!syslogReady) {
    syslogUdp.begin(0);
    syslogReady = true;
  }
}

// Real sender with RFC header
void sendSyslog(const String &msg, int severity) {
  ensureConfigLoaded();
  if (!syslog_enabled || WiFi.status() != WL_CONNECTED) {
    return;
  }
  const uint32_t nowMs = millis();
  if (static_cast<int32_t>(nowMs - syslogBackoffUntilMs) < 0) {
    return;
  }
  if (!syslogReady) {
    initSyslog();
  }
  if (!syslog_enabled || !syslogReady) {
    return;
  }
  if (lastSyslogSendMs != 0 && nowMs - lastSyslogSendMs < SYSLOG_MIN_SEND_INTERVAL_MS) {
    return;
  }

  const int p = pri(SYSLOG_FACILITY, severity);
  const String base = currentHostIdent();
  const String ho =
      syslog_tag.empty() ? base : (syslog_tag.c_str() + String("-") + base);

  String ts = "";
  time_t now = time(nullptr);
  if (now > 1600000000) {
    ts = rfc3164Timestamp() + " ";
  }
  const String header =
      "<" + String(p) + ">" + ts + ho + " " + SYSLOG_APP + ": ";
  const String wire = header + "[" SYSLOG_SECRET "] " + msg;

  if (!syslogUdp.beginPacket(syslogIP, syslog_port)) {
    resetSyslog();
    syslogBackoffUntilMs = nowMs + SYSLOG_ERROR_BACKOFF_MS;
    return;
  }
  const size_t written = syslogUdp.write(reinterpret_cast<const uint8_t *>(wire.c_str()),
                                         wire.length());
  if (written != wire.length() || !syslogUdp.endPacket()) {
    resetSyslog();
    syslogBackoffUntilMs = nowMs + SYSLOG_ERROR_BACKOFF_MS;
    return;
  }
  lastSyslogSendMs = nowMs;
}

// Legacy overload without severity (defaults to info)
void sendSyslog(const String &msg) { sendSyslog(msg, 6); }

void resetSyslog() {
  if (syslogReady) {
    syslogUdp.stop();
    syslogReady = false;
  }
}

#else // !SYSLOG

// No-op definitions so you can build without SYSLOG
void loadSyslogConfig() {}
void initSyslog() {}
void resetSyslog() {}
void sendSyslog(const String &) {}
void sendSyslog(const String &, int) {}

#endif // SYSLOG

