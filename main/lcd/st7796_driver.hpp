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
        ST7796(spi_host_device_t spi_host, uint16_t width, uint16_t height);

        void set_spi_cfg(gpio_num_t mosi_io_num,
                         gpio_num_t clk_io_num,
                         gpio_num_t miso_io_num = GPIO_NUM_NC,
                         int max_transfer_size = 0);
        void set_io_cfg(gpio_num_t cs_gpio_num,
                        gpio_num_t dc_gpio_num,
                        unsigned int clk_frequency);
        void set_panel_cfg(gpio_num_t reset_gpio_num);

        esp_err_t spi_init();
        esp_err_t io_init();
        esp_err_t panel_init();

        esp_lcd_panel_io_handle_t io_handle();
        esp_lcd_panel_handle_t panel_handle();
    };
} // namespace LCD
