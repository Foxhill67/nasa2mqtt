#pragma once

#include <vector>
#include <iostream>

namespace esphome
{
    namespace nasa2mqtt
    {
        extern bool debug_log_messages;
        extern bool debug_log_messages_raw;


        class MessageTarget
        {
        public:
            virtual void register_address(const std::string address) = 0;
        };

        void process_message(std::vector<uint8_t> &data, MessageTarget *target);

        bool is_nasa_address(const std::string &address);

    } // namespace nasa2mqtt
} // namespace esphome