/********************************************************************
 *  mqtt.cpp – Unified MQTT wrapper for ESP8266 (AsyncMqttClient)
 *                         and ESP32   (ESP‑IDF native client)
 *
 *  • Updated for ESP‑IDF v5+ (broker.address.uri replaces host/port)
 *  • RAII‑style holder prevents leaks on error paths
 *  • Simple back‑off reconnect to avoid hammering the broker
 *  • Consistent logging via ESPHome's LOG macros
 *
 *  Author:  <your‑name>
 *  Date:    07 Dec 2025
 ********************************************************************/

#include "esphome/core/log.h"
#include "mqtt.h"

static const char *TAG = "nasa2mqtt.mqtt";

/* ------------------------------------------------------------------
 * Platform‑specific includes & globals
 * ------------------------------------------------------------------ */
#ifdef USE_ESP8266
#include <AsyncMqttClient.h>
static AsyncMqttClient *mqtt_client{nullptr};
#endif

#ifdef USE_ESP32
extern "C" {
    #include "mqtt_client.h"          // ESP‑IDF MQTT API
}
static esp_mqtt_client_handle_t mqtt_client{nullptr};
#endif

namespace esphome {
namespace nasa2mqtt {

/* ------------------------------------------------------------------
 * Helper class – owns the raw client pointer and guarantees cleanup.
 * ------------------------------------------------------------------ */
class MqttClientOwner {
public:
    explicit MqttClientOwner(void *ptr = nullptr) : ptr_(ptr) {}
    ~MqttClientOwner() { reset(); }

    // non‑copyable
    MqttClientOwner(const MqttClientOwner &) = delete;
    MqttClientOwner &operator=(const MqttClientOwner &) = delete;

    // movable
    MqttClientOwner(MqttClientOwner &&other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }
    MqttClientOwner &operator=(MqttClientOwner &&other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    void *get() const { return ptr_; }
    void set(void *p) { ptr_ = p; }

    /** Release ownership without destroying – used when the global
     *  pointer is handed back to the rest of the code. */
    void *release() {
        void *tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }

private:
    void reset() {
        if (!ptr_) return;
#if defined(USE_ESP8266)
        delete static_cast<AsyncMqttClient *>(ptr_);
#elif defined(USE_ESP32)
        esp_mqtt_client_stop(static_cast<esp_mqtt_client_handle_t>(ptr_));
        esp_mqtt_client_destroy(static_cast<esp_mqtt_client_handle_t>(ptr_));
#endif
        ptr_ = nullptr;
    }
    void *ptr_;
};

/* Global owner – ensures a single client instance per run */
static MqttClientOwner client_owner;

/* ------------------------------------------------------------------
 * Simple exponential back‑off for reconnect attempts
 * ------------------------------------------------------------------ */
static unsigned long last_attempt_ms = 0;
static constexpr unsigned long B
