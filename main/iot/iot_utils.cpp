#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"

#include "iot_utils.hpp"

static const char *TAG = "iot_utils";

namespace IOT
{
    esp_err_t upload_file(const char *url,
                          const void *data,
                          size_t len,
                          const char *content_type)
    {
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