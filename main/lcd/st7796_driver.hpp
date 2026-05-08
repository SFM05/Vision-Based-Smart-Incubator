#pragma once

#include <inttypes.h>

#include "driver/spi_master.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_st7796.h"

namespace LCD
{
    class ST7796
    {
    private:
        spi_host_device_t m_spi_host;
        uint16_t m_width, m_height;

        spi_bus_config_t m_bus_cfg;
        esp_lcd_panel_io_handle_t m_io_handle;
        esp_lcd_panel_io_spi_config_t m_io_cfg;
        esp_lcd_panel_handle_t m_panel_handle;
        esp_lcd_panel_dev_config_t m_panel_cfg;

    public:
        /**
         * @brief 构造函数
         *
         * @param spi_host SPI host通道设置
         * @param width LCD宽度
         * @param height LCD高度
         */
        ST7796(spi_host_device_t spi_host, uint16_t width, uint16_t height);

        /**
         * @brief 设置SPI参数
         *
         * @param mosi_io_num MOSI引脚
         * @param clk_io_num CLK引脚
         * @param miso_io_num MISO引脚，默认为不使用（GPIO_NUM_NC）
         * @param max_transfer_size 最大传输字节数，默认为0（自动设置）
         */
        void set_spi_cfg(gpio_num_t mosi_io_num,
                         gpio_num_t clk_io_num,
                         gpio_num_t miso_io_num = GPIO_NUM_NC,
                         int max_transfer_size = 0);
        /**
         * @brief 设置IO参数
         *
         * @param cs_gpio_num CS引脚
         * @param dc_gpio_num DC引脚
         * @param clk_frequency SPI时钟频率
         */
        void set_io_cfg(gpio_num_t cs_gpio_num,
                        gpio_num_t dc_gpio_num,
                        unsigned int clk_frequency);
        /**
         * @brief 设置面板参数
         *
         * @param reset_gpio_num 复位引脚
         */
        void set_panel_cfg(gpio_num_t reset_gpio_num);

        /**
         * @brief 初始化spi
         *
         * @return esp_err_t 是否初始化成功
         */
        esp_err_t spi_init();
        /**
         * @brief 初始化io
         * 
         * @return esp_err_t 是否初始化成功
         */
        esp_err_t io_init();
        /**
         * @brief 初始化面板
         *
         * @return esp_err_t 是否初始化成功
         */
        esp_err_t panel_init();

        /**
         * @brief 获取io句柄
         *
         * @return esp_lcd_panel_io_handle_t io句柄
         */
        esp_lcd_panel_io_handle_t io_handle();
        /**
         * @brief 获取面板句柄
         *
         * @return esp_lcd_panel_handle_t 面板句柄
         */
        esp_lcd_panel_handle_t panel_handle();
    };
} // namespace LCD
