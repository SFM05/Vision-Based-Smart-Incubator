#pragma once

#include "driver/i2c_master.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_ft5x06.h"
#include <cstdint>

namespace LCD
{
    class FT6336
    {
    private:
        uint16_t m_width, m_height;

        i2c_master_bus_config_t m_i2c_cfg;
        i2c_master_bus_handle_t m_i2c_handle;
        esp_lcd_panel_io_i2c_config_t m_io_cfg;
        esp_lcd_panel_io_handle_t m_io_handle;
        esp_lcd_touch_config_t m_touch_cfg;
        esp_lcd_touch_handle_t m_touch_handle;

    public:
        /**
         * @brief 构造函数
         *
         * @param width LCD宽度
         * @param height LCD高度
         */
        FT6336(uint16_t width, uint16_t height, uint32_t speed = 20e3);
        /**
         * @brief 设置I2C
         *
         * @param sda_io_num SDA引脚
         * @param scl_io_num SCL引脚
         */
        void set_i2c_config(gpio_num_t sda_io_num, gpio_num_t scl_io_num);
        /**
         * @brief 设置触摸
         *
         * @param rst_io_num 复位引脚
         * @param int_io_num 中断引脚
         * @param iqr_handle 中断回调函数
         */
        void set_touch_config(gpio_num_t rst_io_num,
                              gpio_num_t int_io_num,
                              esp_lcd_touch_interrupt_callback_t iqr_handle = nullptr,
                              bool mirror_x = false,
                              bool mirror_y = false,
                              bool swap_xy = false);

        /**
         * @brief 初始化I2C
         *
         * @return esp_err_t 是否初始化成功
         */
        esp_err_t i2c_init();
        /**
         * @brief 初始化触摸io
         *
         * @return esp_err_t 是否初始化成功
         */
        esp_err_t io_init();
        /**
         * @brief 获取触摸
         *
         * @return esp_err_t 是否初始化成功
         */
        esp_err_t touch_init();

        /**
         * @brief 获取I2C句柄
         *
         * @return i2c_master_bus_handle_t I2C句柄
         */
        i2c_master_bus_handle_t i2c_handle() const;
        /**
         * @brief 获取触摸io句柄
         *
         * @return esp_lcd_panel_io_handle_t 触摸io句柄
         */
        esp_lcd_panel_io_handle_t io_handle() const;
        /**
         * @brief 获取触摸句柄
         *
         * @return esp_lcd_touch_handle_t 触摸句柄
         */
        esp_lcd_touch_handle_t touch_handle() const;
    };
} /* namespace LCD */
