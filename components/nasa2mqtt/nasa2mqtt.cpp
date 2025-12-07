#include "esphome/core/log.h"
#include "nasa2mqtt.h"
#include "mqtt.h"
#include "util.h"
#include <vector>

namespace esphome
{
  namespace nasa2mqtt
  {
    static const char *TAG = "NASA2MQTT";

    void NASA2MQTT::setup()
    {
      ESP_LOGI(TAG, "setup: Starting MQTT client.");
      // Only start the client once at boot --> doesn't work, crashes ESP32!
      //mqtt_connect(mqtt_host, mqtt_port, mqtt_username, mqtt_password);
    }

    void NASA2MQTT::update()
    {
      ESP_LOGD(TAG, "update: MQTT Connected: %s", (mqtt_connected() ? "YES" : "NO")); // Check status

      mqtt_connect(mqtt_host, mqtt_port, mqtt_username, mqtt_password);

      // Waiting for first update before beginning processing data
      if (data_processing_init)
      {
        ESP_LOGCONFIG(TAG, "Data Processing starting");
        data_processing_init = false;
      }

      std::string knownIndoor = "";
      std::string knownOutdoor = "";
      std::string knownOther = "";
      for (auto const &address : addresses_)
      {
        if (address == "00" || address.rfind("10.", 0) == 0)
        {
          knownOutdoor += knownOutdoor.length() > 0 ? ", " + address : address;
        }
        else if (!is_nasa_address(address) || address.rfind("20.", 0) == 0)
        {
          knownIndoor += knownIndoor.length() > 0 ? ", " + address : address;
        }
        else
        {
          knownOther += knownOther.length() > 0 ? ", " + address : address;
        }
      }
      ESP_LOGCONFIG(TAG, "Discovered devices:");
      ESP_LOGCONFIG(TAG, "  Outdoor: %s", (knownOutdoor.length() == 0 ? "-" : knownOutdoor.c_str()));
      ESP_LOGCONFIG(TAG, "  Indoor:  %s", (knownIndoor.length() == 0 ? "-" : knownIndoor.c_str()));
      if (knownOther.length() > 0)
        ESP_LOGCONFIG(TAG, "  Other:   %s", knownOther.c_str());
    }

    void NASA2MQTT::dump_config()
    {
      ESP_LOGCONFIG(TAG, "NASA2MQTT:");
      this->check_uart_settings(9600, 1, uart::UART_CONFIG_PARITY_EVEN, 8);
    }

    void NASA2MQTT::loop()
    {
      if (data_processing_init)
        return;

      const uint32_t now = millis();
      if (receiving_ && (now - last_transmission_ >= 500))
      {
        ESP_LOGW(TAG, "Last transmission too long ago. Reset RX index.");
        data_.clear();
        receiving_ = false;
      }

      last_transmission_ = now;
      while (available())
      {
        uint8_t c;

        read_byte(&c);
        if (c == 0x32 && !receiving_) // start-byte found
        {
          receiving_ = true;
          bytes_ = 0;
          size_ = 0;
          data_.clear();
        }
        if (receiving_)
        {
          data_.push_back(c);
          bytes_++;
          switch (bytes_)
          {
            case 1: // start byte found
              break;
            case 2: // first part of size found
            {
              size_ = c;
              break;
            }
            case 3: // second part of size found
            {
              size_ = (int)size_ << 8 | c;
              ESP_LOGV(TAG, "Message size in packet: %d", size_);
              break;  
            }
            default: // subsequent bytes 
            {
              if (bytes_ >= (size_+2)) // end byte found
              {  
                receiving_ = false;
                process_message(data_, this);
              }
              break;
            }
          }
        }
      }
    }
  } // namespace nasa2mqtt
} // namespace esphome
