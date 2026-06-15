#ifndef WEB_SERVER_HANDLER_H
#define WEB_SERVER_HANDLER_H

#include <Arduino.h>
#include <user_config.h>

// Forward declaration if ESPAsyncWebServer is used
class ESPAsyncWebServer;

#if defined(WEBSERVER)
void setupWebServer();
void loopWebServer(); // If any loop processing is needed for the web server
void broadcastLog(const String &msg);
void broadcastDevicePosition(const String &id, int position);
void broadcastLastAddress(const String &addr);
void updateTwoWTxStatus(const String &command, const String &result, bool isError = false);
void updateTwoWRxStatus(const String &packetType, const String &from,
                        const String &to, const String &cmd,
                        const String &data, const String &frequency = "");
#else
inline void setupWebServer() {}
inline void loopWebServer() {}
inline void updateTwoWTxStatus(const String &, const String &, bool = false) {}
inline void updateTwoWRxStatus(const String &, const String &, const String &,
                               const String &, const String &, const String & = "") {}
#endif

#endif // WEB_SERVER_HANDLER_H
