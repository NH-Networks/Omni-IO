#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H
#include <user_config.h>

/* MQTT support can be enabled or disabled via the `MQTT` define in
 * `user_config.h`.  When disabled, this header becomes effectively empty so
 * other source files can include it unconditionally. */

#if defined(MQTT)

#include <AsyncMqttClient.h>
#include <ArduinoJson.h>

namespace IOHC {
class iohcPacket;
}

extern AsyncMqttClient mqttClient;
extern const char AVAILABILITY_TOPIC[];

void initMqtt();
void connectToMqtt();
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttMessage(char *topic, char *payload,
                   AsyncMqttClientMessageProperties properties,
                   size_t len, size_t index, size_t total);
void publishDiscovery(const std::string &id, const std::string &name, const std::string &key);
void publishTravelTimeDiscovery(const std::string &id, const std::string &name,
                                const std::string &key, uint32_t travelTime);
void handleMqttConnect();
void publishHeartbeat();
void mqttFuncHandler(const char *cmd);
void publishCoverState(const std::string &id, const char *state);
void publishCoverPosition(const std::string &id, float position);
void publishRadioLogEvent(const IOHC::iohcPacket *packet,
                          const char *direction);
void removeDiscovery(const std::string &id);

#else

inline void publishCoverState(const std::string &, const char *) {}
inline void publishCoverPosition(const std::string &, float) {}
inline void publishDiscovery(const std::string &, const std::string &, const std::string &) {}
inline void publishTravelTimeDiscovery(const std::string &, const std::string &,
                                const std::string &, uint32_t) {}
inline void removeDiscovery(const std::string &) {}
inline void publishHeartbeat() {}
inline void initMqtt() {}
inline void connectToMqtt() {}

#endif // MQTT

#endif // MQTT_HANDLER_H
