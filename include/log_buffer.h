#ifndef LOG_BUFFER_H
#define LOG_BUFFER_H
#include <Arduino.h>
#include <vector>

void addLogMessage(const String &msg);
std::vector<String> getLogMessages();
void setCrashMarker(const char *marker);
String getCrashMarker();

#endif // LOG_BUFFER_H
