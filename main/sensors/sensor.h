#pragma once
/**
 * @file sensor.h
 * @brief SHT40 温湿度 + SGP30 气体传感器驱动 (C 接口)
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#define SENSORS_I2C_FREQ_HZ       100000
#define SENSORS_SHT40_ADDR        0x44
#define SENSORS_SGP30_ADDR        0x58

/**
 * @brief 传感器统一读数结构体
 */
typedef struct {
    float temperature_c;    /**< 温度 (°C) */
    float humidity_rh;      /**< 湿度 (%RH) */
    uint16_t eco2_ppm;      /**< eCO2 (ppm) */
    uint16_t tvoc_ppb;      /**< TVOC (ppb) */
    bool sht_valid;
    bool sgp_valid;          
} sensors_data_t;

/**
 * @brief 初始化 SHT40 和 SGP30（添加设备 + 发送 SGP30 init 命令）
 *        支持部分初始化：任一传感器失败不会阻塞另一个。
 *
 * @param i2c_bus 已创建的 I2C 主设备总线句柄
 * @return esp_err_t 始终返回 ESP_OK（部分失败仅打日志）
 */
esp_err_t sensors_driver_init(i2c_master_bus_handle_t i2c_bus);

/**
 * @brief 读取双传感器数据（仅读取已初始化的传感器）
 *        SGP30 先读，间隔 2ms 后读 SHT40。
 *
 * @param out_data 输出结构体，通过 sht_valid / sgp_valid 判断数据有效性
 * @return ESP_OK  至少一个传感器有有效数据
 * @return ESP_FAIL 所有传感器均无有效数据
 */
esp_err_t sensors_driver_read(sensors_data_t *out_data);

#ifdef __cplusplus
}
#endif
