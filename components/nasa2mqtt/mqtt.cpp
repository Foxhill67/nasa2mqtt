/********************************************************************
 *  mqtt.cpp – Unified MQTT wrapper for ESP8266 (AsyncMqttClient)
 *                         and ESP32   (ESP‑IDF native client)
 *
 *  Updated for ESP‑IDF v5+ (broker.address.uri replaces host/port)
 *  Includes safe cleanup, back‑off reconnect and logging.
 *
 *  Author: <your‑name>
 *  Date:   07 Dec 2025
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

/* Global owner – guarantees a single client instance */
static MqttClientOwner client_owner;

/* ------------------------------------------------------------------
 * Simple exponential back‑off for reconnect attempts
 * ------------------------------------------------------------------ */
static unsigned long last_attempt_ms = 0;
static constexpr unsigned long BASE_DELAY_MS = 2000;   // 2 s
static constexpr unsigned long MAX_DELAY_MS  = 30000;  // 30 s

static bool should_retry() {
    unsigned long now = millis();
    if (now - last_attempt_ms >= BASE_DELAY_MS) {
        last_attempt_ms = now;
        return true;
    }
    return false;
}

/* ------------------------------------------------------------------
 * PUBLIC: Is the client currently connected?
 * ------------------------------------------------------------------ */
bool mqtt_connected() {
#ifdef USE_ESP8266
    if (!mqtt_client) return false;
    return mqtt_client->connected();
#elif defined(USE_ESP32)
    if (!mqtt_client) return false;
    return esp_mqtt_client_get_state(mqtt_client) == MQTT_STATE_CONNECTED;
#else
    return false;
#endif
}

/* ------------------------------------------------------------------
 * PUBLIC: Connect (or reconnect) to the broker
 * ------------------------------------------------------------------ */
void mqtt_connect(const std::string &host,
                  uint16_t            port,
                  const std::string &username,
                  const std::string &password) {
#if defined(USE_ESP8266)

    /* ----- ESP8266 (AsyncMqttClient) ----- */
    if (!mqtt_client) {
        ESP_LOGI(TAG, "Creating AsyncMqttClient instance");
        mqtt_client = new AsyncMqttClient();
        client_owner.set(mqtt_client);               // take ownership

        mqtt_client->setServer(host.c_str(), port);
        if (!username.empty())
            mqtt_client->setCredentials(username.c_str(), password.c_str());
    }

    if (mqtt_client->connected()) {
        ESP_LOGD(TAG, "Already connected to MQTT broker");
        return;
    }

    if (should_retry()) {
        ESP_LOGI(TAG, "Connecting to %s:%u", host.c_str(), port);
        mqtt_client->connect();
    }

#elif defined(USE_ESP32)

    /* ----- ESP32 (ESP‑IDF native client) ----- */
    if (!mqtt_client) {
        ESP_LOGI(TAG, "Initialising ESP‑IDF MQTT client");

        esp_mqtt_client_config_t cfg = {};

        // Build a full URI: mqtt://host:port
        std::string uri = "mqtt://" + host + ":" + std::to_string(port);
        cfg.broker.address.uri = uri.c_str();

        // Optional credentials
        if (!username.empty()) {
            cfg.credentials.username = username.c_str();
            cfg.credentials.authentication.password = password.c_str();
        }

        // You can tweak additional fields here (keepalive, TLS, callbacks…)
        esp_mqtt_client_handle_t client = esp_mqtt_client_init(&cfg);
        if (!client) {
            ESP_LOGE(TAG, "Failed to initialise MQTT client");
            return;
        }

        mqtt_client = client;
        client_owner.set(mqtt_client);               // take ownership

        esp_mqtt_client_start(mqtt_client);
        ESP_LOGI(TAG, "Started MQTT client with URI %s", uri.c_str());
    }

    // Check current state
    esp_mqtt_client_state_t state = esp_mqtt_client_get_state(mqtt_client);
    if (state == MQTT_STATE_CONNECTED) {
        ESP_LOGD(TAG, "Already connected to MQTT broker");
        return;
    }

    // Attempt a reconnect if enough time has elapsed
    if (should_retry()) {
        ESP_LOGI(TAG, "Reconnecting to MQTT broker");
        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_start(mqtt_client);
    }

#else
    #error "Neither USE_ESP8266 nor USE_ESP32 defined – cannot compile MQTT wrapper."
#endif
}

/* ------------------------------------------------------------------
 * PUBLIC: Publish a message (QoS 0, retain false)
 * ------------------------------------------------------------------ */
bool mqtt_publish(const std::string &topic,
                  const std::string &payload) {
    if (!mqtt_connected()) {
        ESP_LOGW(TAG, "Publish attempted while not connected – dropping");
        return false;
    }

#if defined(USE_ESP8266)
    // AsyncMqttClient returns a packet ID (>0) on success, 0 on failure
    uint16_t pkt_id = mqtt_client->publish(
        topic.c_str(),
        0,                 // QoS 0
        false,             // retain
        payload.c_str(),
        payload.size()
    );
    bool ok = (pkt_id != 0);
    ESP_LOGD(TAG, "Publish %s – topic='%s' payload='%s'",
             ok ? "OK" : "FAIL", topic.c_str(), payload.c_str());
    return ok;

#elif defined(USE_ESP32)
    // ESP‑IDF returns message ID >=0 on success, -1 on error
    int msg_id = esp_mqtt_client_publish(
        mqtt_client,
        topic.c_str(),
        payload.c_str(),
        payload.size(),
        0,      // QoS 0
        false   // retain
    );
    bool ok = (msg_id != -1);
    ESP_LOGD(TAG, "Publish %s – topic='%s' payload='%s'",
             ok ? "OK" : "FAIL", topic.c_str(), payload.c_str());
    return ok;

#else
    return false;
#endif
}

/* ------------------------------------------------------------------
 * OPTIONAL: Graceful disconnect (call before shutdown / reset)
 * ------------------------------------------------------------------ */
void mqtt_disconnect() {
#if defined(USE_ESP8266)
    if (mqtt_client) {
        ESP_LOGI(TAG, "Disconnecting AsyncMqttClient");
        mqtt_client->disconnect();
    }
#elif defined(USE_ESP32)
    if (mqtt_client) {
        ESP_LOGI(TAG, "Stopping ESP‑IDF MQTT client");
        esp_mqtt_client_stop(mqtt_client);
    }
#endif
    // Release ownership so the destructor doesn’t double‑free
    client_owner.release();
    mqtt_client = nullptr;
}

} // namespace nasa2mqtt
} // namespace esphome
