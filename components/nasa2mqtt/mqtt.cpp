#include "esphome/core/log.h"
#include "mqtt.h"

#ifdef USE_ESP8266
#include <AsyncMqttClient.h>
AsyncMqttClient *mqtt_client{nullptr};
#endif
#ifdef USE_ESP32
#include <mqtt_client.h>
esp_mqtt_client_handle_t mqtt_client{nullptr};
volatile bool is_mqtt_connected = false; // Define the status variable

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    // Make sure to use the full namespace path to access is_mqtt_connected
    using namespace esphome::nasa2mqtt;
    
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI("NASA2MQTT", "MQTT_EVENT_CONNECTED");
        is_mqtt_connected = true;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW("NASA2MQTT", "MQTT_EVENT_DISCONNECTED");
        is_mqtt_connected = false;
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGV("NASA2MQTT", "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE("NASA2MQTT", "MQTT_EVENT_ERROR, error_code=%d", event->error_handle->esp_error_get_code());
        // Print the underlying TCP/TLS errors for deeper debugging
        if (event->error_handle->error_type == ESP_MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE("NASA2MQTT", "TCP_TRANSPORT Error: %s", esp_err_to_name(event->error_handle->connect_errno));
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}
#endif   

namespace esphome
{
    namespace nasa2mqtt
    {
        bool mqtt_connected()
        {
#ifdef USE_ESP8266
            if (mqtt_client == nullptr)
                return false;

            return mqtt_client->connected();
#elif USE_ESP32
            if (mqtt_client == nullptr)
                return false;

            return is_mqtt_connected; // <-- Use the status variable
#else
            return false;
#endif
        }

void mqtt_connect(const std::string &host, const uint16_t port, const std::string &username, const std::string &password)
        {
#ifdef USE_ESP8266
            if (mqtt_client == nullptr)
            {
                mqtt_client = new AsyncMqttClient();
                mqtt_client->setServer(host.c_str(), port);
                if (username.length() > 0)
                    mqtt_client->setCredentials(username.c_str(), password.c_str());
            }

            if (!mqtt_client->connected())
                mqtt_client->connect();
#elif USE_ESP32
            if (mqtt_client == nullptr)
            {
                esp_mqtt_client_config_t mqtt_cfg = {};

                // --- CORRECTED ACCESS FOR MODERN ESP-IDF (v5.0+) ---

                // 1. Broker Host and Port: Set directly under the 'broker' structure.
                mqtt_cfg.broker.address.hostname = host.c_str(); 
                mqtt_cfg.broker.address.port = port;         

                if (username.length() > 0)
                {
                    // 2. Username: Set directly under 'credentials'.
                    mqtt_cfg.credentials.username = username.c_str(); 

                    // 3. Password: Nested under 'credentials.authentication'.
                    mqtt_cfg.credentials.authentication.password = password.c_str(); 
                }
                
                mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
                esp_mqtt_client_register_event(mqtt_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client);
                esp_mqtt_client_start(mqtt_client);
            }
#endif
        }

        bool mqtt_publish(const std::string &topic, const std::string &payload)
        {
#ifdef USE_ESP8266
            if (mqtt_client == nullptr)
                return false;

            return mqtt_client->publish(topic.c_str(), 0, false, payload.c_str()) != 0;
#elif USE_ESP32
            if (mqtt_client == nullptr)
                return false;

            return esp_mqtt_client_publish(mqtt_client, topic.c_str(), payload.c_str(), payload.length(), 0, false) != -1;
#else
            return true;
#endif
         
        }
    } // namespace nasa2mqtt
} // namespace esphome
