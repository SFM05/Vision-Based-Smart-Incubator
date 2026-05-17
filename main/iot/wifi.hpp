#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <string>

namespace IOT
{
    /* The event group allows multiple bits for each event, but we only care about two events:
     * - we are connected to the AP with an IP
     * - we failed to connect after the maximum amount of retries */
    constexpr int32_t WIFI_CONNECTED_BIT = BIT0;
    constexpr int32_t WIFI_FAIL_BIT = BIT1;

    constexpr uint16_t MAX_RETRY_COUNT = 5;

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

        esp_err_t init();
        esp_err_t connect(const std::string &ssid, const std::string &password);

        const EventBits_t &status() { return m_status; }

        static void wifi_event_handler(void *arg,
                                       esp_event_base_t event_base,
                                       int32_t event_id,
                                       void *event_data);

    private:
        /* FreeRTOS event group to signal when we are connected*/
        EventGroupHandle_t m_wifi_event_group;
        EventBits_t m_status;

        uint16_t m_retry_count;

        Wifi() : m_status{WIFI_FAIL_BIT}, m_retry_count{0}
        {
            m_wifi_event_group = xEventGroupCreate();
        }
        ~Wifi() = default;
    };
} // namespace WIFI
