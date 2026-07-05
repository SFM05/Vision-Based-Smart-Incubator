#pragma once

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动 TCD 温控系统
 * @param sda_pin I2C SDA 引脚
 * @param scl_pin I2C SCL 引脚
 * @param heater_pin 加热片 PWM 引脚
 */
void tcd_start(gpio_num_t sda_pin, gpio_num_t scl_pin, gpio_num_t heater_pin);

#ifdef __cplusplus
}
#endif