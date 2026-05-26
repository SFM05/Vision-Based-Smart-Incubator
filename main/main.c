// main.c（或其他入口文件）
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sensor.h"  // ✅ 包含你的驱动头文件

static const char *TAG = "APP";

void app_main(void)
{
    // 1. 初始化传感器
    ESP_ERROR_CHECK(sensors_driver_init());
    
    // 2. 主循环
    sensors_data_t data;
    
    while (1) {
        if (sensors_driver_read(&data) == ESP_OK && data.is_valid) {
            ESP_LOGI(TAG, "🌡️ %.2f℃ | 💧 %.1f%% | 🌫️ eCO2:%u ppm | TVOC:%u ppb", 
                     data.temperature_c, data.humidity_rh, 
                     data.eco2_ppm, data.tvoc_ppb);
        } else {
            ESP_LOGW(TAG, "Failed to read sensors");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}