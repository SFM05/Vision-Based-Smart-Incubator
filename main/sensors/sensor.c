#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "sensor.h"

static const char *TAG = "SENSORS";

static i2c_master_dev_handle_t s_sht40 = NULL;
static i2c_master_dev_handle_t s_sgp30 = NULL;

static uint8_t crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }
    return crc;
}

esp_err_t sensors_driver_init(i2c_master_bus_handle_t i2c_bus)
{
    i2c_device_config_t sht40_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SENSORS_SHT40_ADDR,
        .scl_speed_hz = SENSORS_I2C_FREQ_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(i2c_bus, &sht40_cfg, &s_sht40);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SHT40(0x44) add device failed: %s", esp_err_to_name(err));
        s_sht40 = NULL;
    }

    i2c_device_config_t sgp30_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SENSORS_SGP30_ADDR,
        .scl_speed_hz = SENSORS_I2C_FREQ_HZ,
    };
    err = i2c_master_bus_add_device(i2c_bus, &sgp30_cfg, &s_sgp30);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SGP30(0x58) add device failed: %s", esp_err_to_name(err));
        s_sgp30 = NULL;
    }

    if (s_sgp30) {
        uint8_t init_cmd[] = {0x20, 0x03};
        err = i2c_master_transmit(s_sgp30, init_cmd, sizeof(init_cmd), pdMS_TO_TICKS(100));
        if (err == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            ESP_LOGW(TAG, "SGP30 init_air_quality failed: %s", esp_err_to_name(err));
        }
    }

    if (s_sht40 || s_sgp30) {
        ESP_LOGI(TAG, "Sensors: SHT40=%s SGP30=%s",
                 s_sht40 ? "OK" : "FAIL", s_sgp30 ? "OK" : "FAIL");
    } else {
        ESP_LOGW(TAG, "No sensor devices available");
    }
    return ESP_OK;
}

esp_err_t sensors_driver_read(sensors_data_t *out_data)
{
    if (!out_data) {
        return ESP_ERR_INVALID_STATE;
    }
    memset(out_data, 0, sizeof(*out_data));

    if (s_sgp30) {
        uint8_t cmd[] = {0x20, 0x08};
        esp_err_t err = i2c_master_transmit(s_sgp30, cmd, sizeof(cmd), pdMS_TO_TICKS(100));
        if (err == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(12));
            uint8_t rx[6] = {};
            err = i2c_master_receive(s_sgp30, rx, sizeof(rx), pdMS_TO_TICKS(100));
            if (err == ESP_OK) {
                if (rx[2] == crc8(rx, 2))
                    out_data->eco2_ppm = ((uint16_t)rx[0] << 8) | rx[1];
                else
                    ESP_LOGW(TAG, "SGP30 CO2 CRC mismatch");
                if (rx[5] == crc8(rx + 3, 2))
                    out_data->tvoc_ppb = ((uint16_t)rx[3] << 8) | rx[4];
                else
                    ESP_LOGW(TAG, "SGP30 TVOC CRC mismatch");
            }
        } else {
            ESP_LOGW(TAG, "SGP30 read failed: %s", esp_err_to_name(err));
        }
    }

    vTaskDelay(pdMS_TO_TICKS(2));

    if (s_sht40) {
        uint8_t cmd = 0xFD;
        esp_err_t err = i2c_master_transmit(s_sht40, &cmd, 1, pdMS_TO_TICKS(100));
        if (err == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(10));
            uint8_t rx[6] = {};
            err = i2c_master_receive(s_sht40, rx, sizeof(rx), pdMS_TO_TICKS(100));
            if (err == ESP_OK) {
                if (rx[2] == crc8(rx, 2)) {
                    uint16_t raw_t = ((uint16_t)rx[0] << 8) | rx[1];
                    out_data->temperature_c = -45.0f + 175.0f * (raw_t / 65535.0f);
                } else {
                    ESP_LOGW(TAG, "SHT40 temperature CRC mismatch");
                }
                if (rx[5] == crc8(rx + 3, 2)) {
                    uint16_t raw_rh = ((uint16_t)rx[3] << 8) | rx[4];
                    out_data->humidity_rh = -6.0f + 125.0f * (raw_rh / 65535.0f);
                } else {
                    ESP_LOGW(TAG, "SHT40 humidity CRC mismatch");
                }
            }
        } else {
            ESP_LOGW(TAG, "SHT40 read failed: %s", esp_err_to_name(err));
        }
    }

    out_data->is_valid = (out_data->eco2_ppm > 0 && out_data->temperature_c != 0.0f);
    return ESP_OK;
}
