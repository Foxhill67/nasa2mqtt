#include "esphome/core/log.h"
#include "protocol.h"
#include "util.h"
#include "nasa.h"

static const char *TAG = "NASA2MQTT";

namespace esphome
{
    namespace nasa2mqtt
    {
        bool debug_log_messages = false;
        bool debug_log_messages_raw = false;

        void process_message(std::vector<uint8_t> &data, MessageTarget *target)
        {
            if (debug_log_messages_raw)
            {
                ESP_LOGW(TAG, "RAW: %s", bytes_to_hex(data).c_str());
            }

//?            if (data.size() == 14)
//?            {
//?                process_non_nasa_message(data, target);
//?                return;
//?            }
            if (data.size() >= 16 && data.size() < 1500)
            {
                process_nasa_message(data, target);
                return;
            }

            ESP_LOGW(TAG, "Unknown message type %s", bytes_to_hex(data).c_str());
        }

        bool is_nasa_address(const std::string &address)
        {
            return address.size() != 2;
        }

//?        Protocol *nasaProtocol = new NasaProtocol();
//?        Protocol *nonNasaProtocol = new NonNasaProtocol();

//?        Protocol *get_protocol(const std::string &address)
//?        {
//?            if (!is_nasa_address(address))
//?                return nonNasaProtocol;

//?            return nasaProtocol;
//?        }
    } // namespace nasa2mqtt
} // namespace esphome