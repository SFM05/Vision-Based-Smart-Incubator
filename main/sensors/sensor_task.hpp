#pragma once
/**
 * @file sensor_task.hpp
 * @brief 传感器 FreeRTOS 读取任务 + LVGL UI 更新
 */

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "ui/gui_guider.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化传感器 I2C 设备（应在 touch.io_init() 之前调用）
 *
 * @param i2c_bus 已创建的 I2C 总线句柄
 * @return esp_err_t
 */
esp_err_t init_sensor_devices(i2c_master_bus_handle_t i2c_bus);

/**
 * @brief 创建传感器读取任务（1 Hz）和 LVGL 定时器（200 ms 刷新 UI）
 *
 * @param ui LVGL 界面指针
 */
void start_sensor_task(lv_ui *ui);

#ifdef __cplusplus
}
#endif
