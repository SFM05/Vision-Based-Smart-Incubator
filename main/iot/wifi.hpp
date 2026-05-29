#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <string>

namespace IOT
{
    class Wifi
    {
    public:
        Wifi(const Wifi &) = delete;
        Wifi &operator=(const Wifi &) = delete;

        static Wifi &get_instance()
        {
            static Wifi instance;
            return instance;
        }

        esp_err_t init_nvs();
        esp_err_t init();
        esp_err_t connect(const std::string &ssid, const std::string &password);

    private:
        Wifi() = default;
        ~Wifi() = default;
    };
} // namespace WIFI
