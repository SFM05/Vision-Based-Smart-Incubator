/**
 * @file lcd_driver.h
 * @brief LCD 显示驱动模块 - EK79007 MIPI DSI 面板接口
 *
 * 封装 ESP-IDF 的 EK79007 组件初始化流程，
 * 提供 init/deinit 接口供主程序调用。
 */

#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 EK79007 LCD 显示屏 (MIPI DSI 接口)
 *
 * 初始化流程:
 *   1. 配置背光 GPIO 并关闭背光
 *   2. 开启 MIPI DSI PHY 电源（LDO）
 *   3. 创建 MIPI DSI 总线（2 lane, 900Mbps）
 *   4. 安装 DBI 面板 IO（用于发送寄存器命令）
 *   5. 创建 DPI 面板配置（RGB565, 1024x600@60Hz）
 *   6. 安装 EK79007 面板驱动
 *   7. 硬件复位 + 初始化面板 + 开启背光
 *
 * @param[out] panel_handle 返回已初始化的面板句柄
 *
 * @return
 *         - ESP_OK: 成功
 *         - ESP_ERR_NOT_SUPPORTED: 当前芯片不支持 MIPI DSI
 *         - 其他: LDO / DSI / DBI / 面板创建失败
 */
esp_err_t lcd_ek79007_init(esp_lcd_panel_handle_t *panel_handle);

/**
 * @brief 反初始化并释放 LCD 资源
 *
 * 执行:
 *   - 删除面板驱动（释放 DPI/DSI 资源）
 *   - 释放 MIPI DSI PHY LDO 电源通道
 *
 * @param[in] panel_handle 面板句柄（可为 NULL，内部会做空指针检查）
 *
 * @return ESP_OK 始终返回成功
 */
esp_err_t lcd_ek79007_deinit(esp_lcd_panel_handle_t panel_handle);

#ifdef __cplusplus
}
#endif

#endif /* LCD_DRIVER_H */
