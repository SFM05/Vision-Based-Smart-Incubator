// sensors_driver.c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2c_master.h"
#include "sensor.h"  // ✅ 包含自己的头文件

// ================= 内部私有变量（外部不可见） =================
static const char *TAG = "SENSORS";  // ✅ 与头文件 get_tag() 保持一致

static i2c_master_bus_handle_t s_i2c_bus;      // ✅ 加 s_ 前缀表示 static
static i2c_master_dev_handle_t s_sht40_dev;
static i2c_master_dev_handle_t s_sgp30_dev;
static bool s_is_initialized = false;          // ✅ 初始化状态标记

// ================= 内部工具函数（static，不暴露） =================
static uint8_t calc_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
        }
    }
    return crc;
}

// ================= 公共接口实现 =================

esp_err_t sensors_driver_init(void)
{
    if (s_is_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing SHT40/SGP30 sensors");

    // 1. 创建 I2C 总线
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_XTAL,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = SENSORS_I2C_SCL_PIN,
        .sda_io_num = SENSORS_I2C_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_i2c_bus), TAG, "I2C bus create failed");

    // 2. 挂载设备
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = SENSORS_I2C_FREQ_HZ,
    };

    dev_cfg.device_address = SENSORS_SHT40_ADDR;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &s_sht40_dev), TAG, "SHT40 add failed");

    dev_cfg.device_address = SENSORS_SGP30_ADDR;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &s_sgp30_dev), TAG, "SGP30 add failed");

    // 3. SGP30 上电初始化
    uint8_t init_cmd[] = {0x20, 0x03};
    ESP_RETURN_ON_ERROR(i2c_master_transmit(s_sgp30_dev, init_cmd, sizeof(init_cmd), pdMS_TO_TICKS(100)), TAG, "SGP30 init failed");
    vTaskDelay(pdMS_TO_TICKS(15));

    s_is_initialized = true;
    ESP_LOGI(TAG, "✅ Sensors initialized");
    return ESP_OK;
}

esp_err_t sensors_driver_read(sensors_data_t *out_data)
{
    if (!s_is_initialized || !out_data) {
        return ESP_ERR_INVALID_STATE;
    }

    // 初始化输出为无效
    out_data->is_valid = false;

    // === 读取 SGP30 ===
    uint8_t sgp_cmd[] = {0x20, 0x08};
    uint8_t sgp_buf[6];
    
    if (i2c_master_transmit(s_sgp30_dev, sgp_cmd, sizeof(sgp_cmd), pdMS_TO_TICKS(100)) == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(15));
        if (i2c_master_receive(s_sgp30_dev, sgp_buf, 6, pdMS_TO_TICKS(100)) == ESP_OK) {
            if (calc_crc8(sgp_buf, 2) == sgp_buf[2] && 
                calc_crc8(&sgp_buf[3], 2) == sgp_buf[5]) {
                out_data->eco2_ppm = (sgp_buf[0] << 8) | sgp_buf[1];
                out_data->tvoc_ppb = (sgp_buf[3] << 8) | sgp_buf[4];
            }
        }
    }

    // ⭐ 总线隔离延时
    vTaskDelay(pdMS_TO_TICKS(2));

    // === 读取 SHT40 ===
    uint8_t sht_cmd = 0xFD;
    uint8_t sht_buf[6];
    
    if (i2c_master_transmit(s_sht40_dev, &sht_cmd, 1, pdMS_TO_TICKS(100)) == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(10));
        if (i2c_master_receive(s_sht40_dev, sht_buf, 6, pdMS_TO_TICKS(100)) == ESP_OK) {
            if (calc_crc8(sht_buf, 2) == sht_buf[2] && 
                calc_crc8(&sht_buf[3], 2) == sht_buf[5]) {
                uint16_t rt = (sht_buf[0] << 8) | sht_buf[1];
                uint16_t rh = (sht_buf[3] << 8) | sht_buf[4];
                out_data->temperature_c = -45.0f + 175.0f * (float)rt / 65535.0f;
                out_data->humidity_rh   = -6.0f  + 125.0f * (float)rh  / 65535.0f;
            }
        }
    }

    // ✅ 标记数据有效（实际项目中可分别标记每个字段）
    out_data->is_valid = true;
    return ESP_OK;
}

const char* sensors_driver_get_tag(void) {
    return TAG;
}