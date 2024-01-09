#pragma once

#include <set>
#include <map>
#include <optional>
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "protocol.h"

namespace esphome
{
  namespace nasa2mqtt
  {
    class NasaProtocol;

    class NASA2MQTT : public PollingComponent,
                       public uart::UARTDevice,
                       public MessageTarget
    {
    public:
      NASA2MQTT() = default;

      void setup() override;
      void update() override;
      void loop() override;
      void dump_config() override;

      void register_address(const std::string address) override
      {
        addresses_.insert(address);
      }

      void set_mqtt(std::string host, int port, std::string username, std::string password)
      {
       mqtt_host = host;
       mqtt_port = port;
       mqtt_username = username;
       mqtt_password = password;
      }

      void set_debug_log_messages(bool value)
      {
        debug_log_messages = value;
      }

      void set_debug_log_messages_raw(bool value)
      {
        debug_log_messages_raw = value;
      }

      std::set<std::string> addresses_;

      std::vector<uint8_t> data_;
      bool receiving_{false};
      uint32_t last_transmission_{0};
      uint16_t bytes_ = 0;
      uint16_t size_ = 0;
      bool data_processing_init = true;

      // settings from yaml
      std::string mqtt_host = "";
      uint16_t mqtt_port = 1883;
      std::string mqtt_username = "";
      std::string mqtt_password = "";
    };

  } // namespace nasa2mqtt
} // namespace esphome
