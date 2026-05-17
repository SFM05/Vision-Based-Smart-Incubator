#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <format>
#include <string.h>
#include <string>
#include <time.h>

// #include "lwip/err.h"
// #include "lwip/sys.h"

#include "nvs_init.hpp"
#include "wifi.hpp"

const char *TAG = "WIFI";

namespace IOT
{
    void Wifi::wifi_event_handler(void *arg,
                                  esp_event_base_t event_base,
                                  int32_t event_id,
                                  void *event_data)
    {
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
        {
            esp_wifi_connect();
        }
        else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            if (Wifi::get_instance().m_retry_count < MAX_RETRY_COUNT)
            {
                esp_wifi_connect();
                Wifi::get_instance().m_retry_count++;
                ESP_LOGI(TAG, "Retry connecting to AP.");
            }
            else
            {
                xEventGroupSetBits(Wifi::get_instance().m_wifi_event_group,
                                   WIFI_FAIL_BIT);
            }
            ESP_LOGI(TAG, "Connect to AP failed.");
        }
        else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
        {
            // auto event = static_cast<ip_event_got_ip_t *>(event_data);
            // std::string msg = std::format("Connect success, ip: {}", ""/*IP2STR(&event->ip_info.ip)*/);
            std::string msg = std::format("");
            // ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
            Wifi::get_instance().m_retry_count = 0;
            xEventGroupSetBits(Wifi::get_instance().m_wifi_event_group,
                               WIFI_CONNECTED_BIT);
        }
    }

    esp_err_t Wifi::init()
    {
        if (!NVS_init::is_initialized) [[unlikely]]
        {
            ESP_LOGE(TAG, "NVS is not initialized.");
            return ESP_FAIL;
        }

        esp_err_t err = esp_netif_init();

        err = esp_event_loop_create_default();
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;
        err = esp_event_handler_instance_register(WIFI_EVENT,
                                                  ESP_EVENT_ANY_ID,
                                                  &wifi_event_handler,
                                                  NULL,
                                                  &instance_any_id);
        err = esp_event_handler_instance_register(IP_EVENT,
                                                  IP_EVENT_STA_GOT_IP,
                                                  &wifi_event_handler,
                                                  NULL,
                                                  &instance_got_ip);

        return err;
    }

    esp_err_t Wifi::connect(const std::string &ssid, const std::string &password)
    {
        wifi_config_t wifi_config{};
        memcpy(wifi_config.sta.ssid, ssid.c_str(), ssid.size() + 1);
        memcpy(wifi_config.sta.password, password.c_str(), password.size() + 1);
        /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
         * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
         * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
         * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
         */
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        wifi_config.sta.pmf_cfg.capable = true;
        wifi_config.sta.pmf_cfg.required = false;
        // wifi_config.sta.sae_pwe_h2e = ESP_WIFI_SAE_MODE;
        // wifi_config.sta.sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER;

        esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
        err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        err = esp_wifi_start();

        ESP_LOGI(TAG, "wifi_init_sta finished.");

        /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
         * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
        m_status = xEventGroupWaitBits(m_wifi_event_group,
                                       WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                       pdFALSE,
                                       pdFALSE,
                                       portMAX_DELAY);

        /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
         * happened. */
        if (m_status & WIFI_CONNECTED_BIT)
            ESP_LOGI(TAG, "Connect success.");
        else if (m_status & WIFI_FAIL_BIT)
            ESP_LOGI(TAG, "Connect failed.");
        else
            ESP_LOGE(TAG, "Unexpected error occurred.");

        return err;
    }
} // namespace WIFI