#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <format>
#include <string.h>
#include <string>
#include <time.h>

// #include "lwip/err.h"
// #include "lwip/sys.h"

#include "esp_netif_sntp.h"

#include "wifi.hpp"

const char *TAG = "WIFI";

namespace IOT
{
    /* FreeRTOS event group to signal when we are connected*/
    static EventGroupHandle_t s_wifi_event_group;

    /* The event group allows multiple bits for each event, but we only care about two events:
     * - we are connected to the AP with an IP
     * - we failed to connect after the maximum amount of retries */
    constexpr int32_t WIFI_CONNECTED_BIT = BIT0;
    constexpr int32_t WIFI_FAIL_BIT = BIT1;

    constexpr uint16_t MAX_RETRY_COUNT = 5;
    static uint16_t retry_count = 0;

    static void wifi_event_handler(void *arg,
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
            if (retry_count < MAX_RETRY_COUNT)
            {
                esp_wifi_connect();
                retry_count++;
                ESP_LOGI(TAG, "Retry connecting to AP.");
            }
            else
            {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
            ESP_LOGI(TAG, "Connect to AP failed.");
        }
        else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
        {
            // auto event = static_cast<ip_event_got_ip_t *>(event_data);
            // std::string msg = std::format("Connect success, ip: {}", ""/*IP2STR(&event->ip_info.ip)*/);
            std::string msg = std::format("");
            // ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
            retry_count = 0;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }

    esp_err_t Wifi::init_nvs()
    {
        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            ESP_LOGE(TAG, "NVS Flash initialize failed. Erase it to try again.");
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
        }

        if (err == ESP_OK)
            ESP_LOGI(TAG, "NVS Flash initialized.");
        else
            ESP_LOGE(TAG, "NVS Flash initialize failed.");

        return err;
    }

    esp_err_t Wifi::init()
    {
        s_wifi_event_group = xEventGroupCreate();
        esp_err_t err;

        err = esp_netif_init();

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
        esp_err_t err;

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

        err = esp_wifi_set_mode(WIFI_MODE_STA);
        err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        err = esp_wifi_start();

        ESP_LOGI(TAG, "wifi_init_sta finished.");

        /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
         * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                               WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                               pdFALSE,
                                               pdFALSE,
                                               portMAX_DELAY);

        /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
         * happened. */
        if (bits & WIFI_CONNECTED_BIT)
            ESP_LOGI(TAG, "Connect success.");
        else if (bits & WIFI_FAIL_BIT)
            ESP_LOGI(TAG, "Connect failed.");
        else
            ESP_LOGE(TAG, "Unexpected error occurred.");

        return err;
    }

    esp_err_t Wifi::ntp_sync()
    {
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        esp_netif_sntp_init(&config);
        auto err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000));
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "SNTP failed to sync time within 10s.");
        }

        // 设置时区：UTC+8
        setenv("TZ", "CST-8", 1);
        tzset();

        time_t now = time(NULL); // 秒级时间戳
        struct tm tm_info;
        localtime_r(&now, &tm_info); // 转本地时间
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_info);
        ESP_LOGI(TAG, "SNTP sync time success, current time: %s", buffer);

        return err;
    }
} // namespace WIFI