#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动 TCD 温控系统
 * @param i2c_bus 已初始化的 I2C 总线句柄（SHT40 应已注册到此总线）
 * @param heater_pin 加热片 PWM 引脚
 */
void tcd_start(i2c_master_bus_handle_t i2c_bus, gpio_num_t heater_pin);

/**
 * @brief 动态设置目标温度
 * @param target_c 目标温度 (°C)
 */
void tcd_set_target(float target_c);

#ifdef __cplusplus
}
#endif