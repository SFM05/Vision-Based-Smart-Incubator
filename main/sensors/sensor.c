#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2c_master.h"
#include "sensor.h"

static const char *TAG = "SENSORS";

static i2c_master_dev_handle_t s_sht40_dev;
static i2c_master_dev_handle_t s_sgp30_dev;
static bool s_is_initialized = false;

static uint8_t calc_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
        }
    }
    return crc;
}

esp_err_t sensors_driver_init(i2c_master_bus_handle_t i2c_bus)
{
    if (s_is_initialized) {
        return ESP_OK;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = SENSORS_I2C_FREQ_HZ,
    };

    dev_cfg.device_address = SENSORS_SHT40_ADDR;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(i2c_bus, &dev_cfg, &s_sht40_dev), TAG, "SHT40 add failed");

    dev_cfg.device_address = SENSORS_SGP30_ADDR;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(i2c_bus, &dev_cfg, &s_sgp30_dev), TAG, "SGP30 add failed");

    uint8_t init_cmd[] = {0x20, 0x03};
    ESP_RETURN_ON_ERROR(i2c_master_transmit(s_sgp30_dev, init_cmd, sizeof(init_cmd), pdMS_TO_TICKS(100)), TAG, "SGP30 init failed");
    vTaskDelay(pdMS_TO_TICKS(15));

    s_is_initialized = true;
    ESP_LOGI(TAG, "SHT40+SGP30 initialized");
    return ESP_OK;
}

esp_err_t sensors_driver_read(sensors_data_t *out_data)
{
    if (!s_is_initialized || !out_data) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(out_data, 0, sizeof(*out_data));
    bool sgp_ok = false, sht_ok = false;

    uint8_t sgp_cmd[] = {0x20, 0x08};
    uint8_t sgp_buf[6];

    esp_err_t err = i2c_master_transmit(s_sgp30_dev, sgp_cmd, sizeof(sgp_cmd), pdMS_TO_TICKS(100));
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(15));
        err = i2c_master_receive(s_sgp30_dev, sgp_buf, 6, pdMS_TO_TICKS(100));
        if (err == ESP_OK) {
            uint8_t c_crc = calc_crc8(sgp_buf, 2);
            uint8_t t_crc = calc_crc8(&sgp_buf[3], 2);
            if (c_crc == sgp_buf[2] && t_crc == sgp_buf[5]) {
                out_data->eco2_ppm = (sgp_buf[0] << 8) | sgp_buf[1];
                out_data->tvoc_ppb = (sgp_buf[3] << 8) | sgp_buf[4];
                sgp_ok = true;
            } else {
                ESP_LOGW(TAG, "SGP30 CRC: CO2=0x%02X/0x%02X TVOC=0x%02X/0x%02X",
                         c_crc, sgp_buf[2], t_crc, sgp_buf[5]);
            }
        } else {
            ESP_LOGW(TAG, "SGP30 receive failed: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(TAG, "SGP30 transmit failed: %s", esp_err_to_name(err));
    }

    vTaskDelay(pdMS_TO_TICKS(2));

    uint8_t sht_cmd = 0xFD;
    uint8_t sht_buf[6];

    err = i2c_master_transmit(s_sht40_dev, &sht_cmd, 1, pdMS_TO_TICKS(100));
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(10));
        err = i2c_master_receive(s_sht40_dev, sht_buf, 6, pdMS_TO_TICKS(100));
        if (err == ESP_OK) {
            uint8_t t_crc = calc_crc8(sht_buf, 2);
            uint8_t rh_crc = calc_crc8(&sht_buf[3], 2);
            if (t_crc == sht_buf[2] && rh_crc == sht_buf[5]) {
                uint16_t rt = (sht_buf[0] << 8) | sht_buf[1];
                uint16_t rh = (sht_buf[3] << 8) | sht_buf[4];
                out_data->temperature_c = -45.0f + 175.0f * (float)rt / 65535.0f;
                out_data->humidity_rh   = -6.0f  + 125.0f * (float)rh  / 65535.0f;
                sht_ok = true;
            } else {
                ESP_LOGW(TAG, "SHT40 CRC: T=0x%02X/0x%02X RH=0x%02X/0x%02X",
                         t_crc, sht_buf[2], rh_crc, sht_buf[5]);
            }
        } else {
            ESP_LOGW(TAG, "SHT40 receive failed: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(TAG, "SHT40 transmit failed: %s", esp_err_to_name(err));
    }

    out_data->sht_valid = sht_ok;
    out_data->sgp_valid = sgp_ok;
    return (sht_ok && sgp_ok) ? ESP_OK : ESP_FAIL;
}
