#include "sensor_task.hpp"
#include "sensor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lvgl.h"
#include "iot/iot_utils.hpp"

static const char *TAG = "SensorTask";

static struct {
    SemaphoreHandle_t mutex;
    float temperature;
    float humidity;
    uint16_t co2;
    bool has_new_data;
} s_state = {};

static float s_target_temperature = 27.0f;

static lv_ui *s_ui = nullptr;

static void sensor_read_task_fn(void *arg)
{
    (void)arg;
    while (true) {
        sensors_data_t data;
        esp_err_t err = sensors_driver_read(&data);
        if (err == ESP_OK && data.is_valid) {
            if (xSemaphoreTake(s_state.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                s_state.temperature = data.temperature_c;
                s_state.humidity = data.humidity_rh;
                s_state.co2 = data.eco2_ppm;
                s_state.has_new_data = true;
                xSemaphoreGive(s_state.mutex);
            }
            IOT::report_env(data.temperature_c, data.humidity_rh, data.eco2_ppm);

            ESP_LOGI(TAG, "T=%.1fC RH=%.1f%% CO2=%dppm TVOC=%dppb",
                     (double)data.temperature_c, (double)data.humidity_rh,
                     data.eco2_ppm, data.tvoc_ppb);
        } else {
            ESP_LOGW(TAG, "Read failed (SGP30=%s SHT40=%s)",
                     data.eco2_ppm ? "OK" : "FAIL",
                     data.temperature_c != 0 ? "OK" : "FAIL");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void ui_update_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!s_state.has_new_data || !s_ui) return;

    if (xSemaphoreTake(s_state.mutex, pdMS_TO_TICKS(50)) != pdTRUE) return;

    float temp = s_state.temperature;
    float humid = s_state.humidity;
    uint16_t co2 = s_state.co2;
    s_state.has_new_data = false;
    xSemaphoreGive(s_state.mutex);

    char buf[32];

    if (s_ui->screen_2_label_1) {
        int c = (int)(temp * 10 + 0.5f);
        int s = (int)(s_target_temperature * 10 + 0.5f);
        lv_snprintf(buf, sizeof(buf), "Curr: %d.%d C    Set: %d.%d C",
                    c / 10, c % 10, s / 10, s % 10);
        lv_label_set_text(s_ui->screen_2_label_1, buf);
    }
    if (s_ui->screen_2_label_2) {
        int h = (int)(humid * 10 + 0.5f);
        lv_snprintf(buf, sizeof(buf), "Humidity: %d.%d %%", h / 10, h % 10);
        lv_label_set_text(s_ui->screen_2_label_2, buf);
    }
    if (s_ui->screen_2_label_4) {
        lv_snprintf(buf, sizeof(buf), "CO2: %d ppm", co2);
        lv_label_set_text(s_ui->screen_2_label_4, buf);
    }
}

esp_err_t init_sensor_devices(i2c_master_bus_handle_t i2c_bus)
{
    s_state.mutex = xSemaphoreCreateMutex();
    return sensors_driver_init(i2c_bus);
}

void start_sensor_task(lv_ui *ui)
{
    s_ui = ui;

    lv_timer_create(ui_update_timer_cb, 200, NULL);

    xTaskCreatePinnedToCore(
        sensor_read_task_fn, "sensor_read", 4096, NULL, 5, NULL, 0);

    ESP_LOGI(TAG, "Sensor task started (interval: 1s)");
}

float sensor_get_target_temperature(void) { return s_target_temperature; }
void sensor_set_target_temperature(float temp) { s_target_temperature = temp; }

float sensor_get_current_temperature(void)
{
    float t = 0.0f;
    if (xSemaphoreTake(s_state.mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        t = s_state.temperature;
        xSemaphoreGive(s_state.mutex);
    }
    return t;
}
