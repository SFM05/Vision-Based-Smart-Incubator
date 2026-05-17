#include "nvs_init.hpp"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_err.h"

static const char *TAG = "NVS";

namespace NVS_init
{
    esp_err_t nvs_init()
    {
        if (is_initialized) [[unlikely]]
        {
            ESP_LOGW(TAG, "NVS has been initialized.");
            return ESP_OK;
        }

        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            ESP_LOGE(TAG, "NVS Flash initialize failed. Erase it to try again.");
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
        }

        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "NVS Flash initialized.");
            is_initialized = true;
        }
        else
            ESP_LOGE(TAG, "NVS Flash initialize failed.");

        return err;
    }
}