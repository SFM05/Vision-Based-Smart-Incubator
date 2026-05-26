// sensors_driver.h
#pragma once  // ✅ 防重复包含（也可用 #ifndef 风格）

#ifdef __cplusplus
extern "C" {  // ✅ 兼容 C++ 调用
#endif

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// ================= 配置参数（可外部修改） =================
#define SENSORS_I2C_SDA_PIN       4
#define SENSORS_I2C_SCL_PIN       5
#define SENSORS_I2C_FREQ_HZ       100000

#define SENSORS_SHT40_ADDR        0x44
#define SENSORS_SGP30_ADDR        0x58

// ================= 数据结构：传感器读数 =================
typedef struct {
    float temperature_c;    // 温度 (°C)
    float humidity_rh;      // 湿度 (%RH)
    uint16_t eco2_ppm;      // eCO2 (ppm)
    uint16_t tvoc_ppb;      // TVOC (ppb)
    bool is_valid;          // 数据是否有效（校验通过）
} sensors_data_t;

// ================= 公共 API 声明 =================

/**
 * @brief 初始化 I2C 总线 + 挂载 SHT40/SGP30 设备
 * @return ESP_OK 成功，其他为错误码
 * @note 只需调用一次，建议在 app_main 开头执行
 */
esp_err_t sensors_driver_init(void);

/**
 * @brief 读取一次传感器数据（阻塞式）
 * @param out_data 输出参数：存储读取结果
 * @return ESP_OK 成功，ESP_FAIL 校验失败或通信错误
 * @note 内部已处理 SGP30/SHT40 时序隔离，直接循环调用即可
 */
esp_err_t sensors_driver_read(sensors_data_t *out_data);

/**
 * @brief 获取传感器标签（用于日志输出）
 * @return 常量字符串 "SENSORS"
 */
const char* sensors_driver_get_tag(void);

#ifdef __cplusplus
}
#endif