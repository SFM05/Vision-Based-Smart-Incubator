#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"

#include "esp_netif_sntp.h"

#include "wifi.hpp"

#include "iot_utils.hpp"

static const char *TAG = "iot_utils";

namespace IOT
{
    esp_err_t ntp_time_sync()
    {
        if (Wifi::get_instance().status() != WIFI_CONNECTED_BIT) [[unlikely]]
        {
            ESP_LOGE(TAG, "Wifi isn't connected");
            return ESP_FAIL;
        }

        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        esp_err_t err = esp_netif_sntp_init(&config);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Unable to init SNTP server.");
            return err;
        }

        err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000));
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "SNTP failed to sync time within 10s.");
            return err;
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

    esp_err_t upload_file(const char *url,
                          const void *data,
                          size_t len)
    {
        if (Wifi::get_instance().status() != WIFI_CONNECTED_BIT) [[unlikely]]
        {
            ESP_LOGE(TAG, "Wifi ism't connected.");
            return ESP_FAIL;
        }

        esp_http_client_config_t config{};
        config.url = url;
        config.method = HTTP_METHOD_PUT;
        config.timeout_ms = 10000;
        config.crt_bundle_attach = esp_crt_bundle_attach;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client)
        {
            ESP_LOGE(TAG, "Failed to init HTTP client");
            return ESP_FAIL;
        }

        // Streaming upload: open connection then write data
        esp_err_t err = esp_http_client_open(client, len);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
            esp_http_client_cleanup(client);
            return err;
        }

        int written = esp_http_client_write(client, static_cast<const char *>(data), len);
        if (written < 0 || (size_t)written != len)
        {
            ESP_LOGE(TAG, "Write failed: wrote %d/%zu", written, len);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }

        int64_t content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0)
        {
            ESP_LOGE(TAG, "Failed to fetch response headers");
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }

        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "Upload response: %d", status_code);

        esp_http_client_close(client);
        esp_http_client_cleanup(client);

        return (status_code == 200 || status_code == 201) ? ESP_OK : ESP_FAIL;
    }
}