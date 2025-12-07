#pragma once
#include <string>

namespace esphome {
namespace nasa2mqtt {

/// Returns true if the MQTT client is currently connected.
bool mqtt_connected();

/// Initialise (or reconnect) the client to the given broker.
void mqtt_connect(const std::string &host,
                  uint16_t            port,
                  const std::string &username,
                  const std::string &password);

/// Publish a payload on a topic (QoS 0, retain false). Returns true on success.
bool mqtt_publish(const std::string &topic,
                  const std::string &payload);

/// Optional: cleanly stop the client before a reboot or deep‑sleep.
void mqtt_disconnect();

} // namespace nasa2mqtt
} // namespace esphome
