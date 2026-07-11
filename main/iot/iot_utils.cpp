#include "esp_heap_caps.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
namespace fs = std::filesystem;
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif_sntp.h"
#include "freertos/projdefs.h"
#include <functional>
#include <sys/types.h>

#include "eth.hpp"
#include "mqtt.hpp"
#include "mqtt_client.h"

#include "iot_utils.hpp"

static const char *TAG = "iot_utils";
constexpr size_t HTTP_BUFFER_SIZE = 1024;
static auto esp_deleter = [](uint8_t *ptr)
{ heap_caps_free(ptr); };
using esp_deleter_type = std::function<void(uint8_t *)>;

namespace IOT
{
    esp_err_t ntp_time_sync()
    {
        if (!ETH::Ethernet::instance().status()) [[unlikely]]
        {
            ESP_LOGE(TAG, "Ethernet not connected.");
            return ESP_FAIL;
        }

        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        esp_err_t err = esp_netif_sntp_init(&config);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Unable to init SNTP server.");
            return err;
        }

        uint16_t retry = 1;
        constexpr uint16_t retry_count = 15;
        do
        {
            err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000));
            if (err == ESP_OK)
                break;
            ESP_LOGE(TAG, "SNTP failed to sync time within 10s, retry %d/%d", retry, retry_count);
            retry++;
        } while (retry <= retry_count);
        if (err != ESP_OK) [[unlikely]]
        {
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
        if (!ETH::Ethernet::instance().status()) [[unlikely]]
        {
            ESP_LOGE(TAG, "Ethernet not connected.");
            return ESP_FAIL;
        }

        esp_http_client_config_t config{};
        config.url = url;
        config.method = HTTP_METHOD_PUT;
        config.timeout_ms = 10000;
#if not CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY
        config.crt_bundle_attach = esp_crt_bundle_attach;
#endif

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client)
        {
            ESP_LOGE(TAG, "Failed to init HTTP client: %s", url);
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

    esp_err_t upload_file(const char *url,
                          const char *path,
                          uint8_t *buffer,
                          size_t size)
    {
        fs::path filename{path};

        // 获取文件大小
        if (!(fs::exists(filename) && fs::is_regular_file(path))) [[unlikely]]
        {
            ESP_LOGE(TAG, "The file does not exist or is not a regular file: %s", path);
            return ESP_FAIL;
        }
        size_t filesize = fs::file_size(filename);

        // 打开文件
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) [[unlikely]]
        {
            ESP_LOGE(TAG, "Cannot open file: %s", path);
            return ESP_FAIL;
        }

        // 创建http客户端
        esp_http_client_config_t config{};
        config.url = url;
        config.method = HTTP_METHOD_PUT;
        config.timeout_ms = 10000;
#if not CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY
        config.crt_bundle_attach = esp_crt_bundle_attach;
#endif

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client)
        {
            ESP_LOGE(TAG, "Failed to init HTTP client: %s", url);
            return ESP_FAIL;
        }

        // Streaming upload: open connection then write data
        esp_err_t err = esp_http_client_open(client, filesize);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
            esp_http_client_cleanup(client);
            return err;
        }

        bool need_free = false;
        if (buffer == nullptr) [[unlikely]]
        {
            buffer = static_cast<uint8_t *>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM));
            need_free = true;
        }
        size_t total_posted = 0;
        do
        {
            int read_size = std::min(HTTP_BUFFER_SIZE, filesize - total_posted);
            file.read(reinterpret_cast<char *>(buffer), read_size);
            if (file.gcount() == 0)
                break;

            int wlen = esp_http_client_write(client,
                                             reinterpret_cast<char *>(buffer),
                                             read_size);
            if (wlen < 0)
            {
                ESP_LOGE(TAG, "Write failed!");
                if (need_free) [[unlikely]]
                    heap_caps_free(buffer);
                return ESP_FAIL;
            }
            total_posted += wlen;
        } while (total_posted < filesize);

        esp_http_client_fetch_headers(client);
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP Status = %d, posted = %d bytes", status_code, total_posted);
        esp_http_client_perform(client);

        esp_http_client_cleanup(client);
        if (need_free) [[unlikely]]
            heap_caps_free(buffer);
        return ESP_OK;
    }

    esp_err_t report_env(float temp, float hum)
    {
        if (!MQTT::MQTT::instance().mqtt_handle())
        {
            return ESP_FAIL;
        }

        time_t now = time(NULL);
        struct tm tm_info;
        localtime_r(&now, &tm_info);
        char time[20];
        strftime(time, sizeof(time), "%Y%m%d-%H%M%S", &tm_info);
        if (time[0] != '2')
            memset(time, 0, sizeof(time));

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "timestamp", time);
        cJSON_AddNumberToObject(root, "temp", temp);
        cJSON_AddNumberToObject(root, "hum", hum);
        char *json_str = cJSON_PrintUnformatted(root);

        uint8_t mac[6];
        esp_efuse_mac_get_default(mac);
        int id = esp_mqtt_client_publish(MQTT::MQTT::instance().mqtt_handle(),
                                         std::format("device/{}/data", get_mac()).c_str(),
                                         json_str,
                                         0,
                                         1,
                                         0);

        cJSON_free(json_str);
        cJSON_Delete(root);

        if (id < 0) [[unlikely]]
        {
            return ESP_FAIL;
        }
        return ESP_OK;
    }
}