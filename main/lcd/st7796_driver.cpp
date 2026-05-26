#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "soc/soc_caps.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"

#include "esp_lcd_st7796.h"
#include "st7796_driver.hpp"

static const char *TAG = "ST7796";

namespace LCD
{
    ST7796::ST7796(spi_host_device_t spi_host,
                   uint16_t width,
                   uint16_t height) : m_spi_host{spi_host},
                                      m_width{width},
                                      m_height{height},
                                      m_bus_cfg{},
                                      m_io_handle{nullptr},
                                      m_io_cfg{},
                                      m_panel_handle{nullptr},
                                      m_panel_cfg{}
    {
    }

    void ST7796::set_spi_cfg(gpio_num_t mosi_io_num,
                             gpio_num_t clk_io_num,
                             gpio_num_t miso_io_num,
                             int max_transfer_size)
    {
        m_bus_cfg.mosi_io_num = mosi_io_num;
        m_bus_cfg.miso_io_num = miso_io_num;
        m_bus_cfg.sclk_io_num = clk_io_num;
        m_bus_cfg.quadwp_io_num = -1;
        m_bus_cfg.quadhd_io_num = -1;
        m_bus_cfg.max_transfer_sz = max_transfer_size;
    }
    void ST7796::set_io_cfg(gpio_num_t cs_gpio_num,
                            gpio_num_t dc_gpio_num,
                            unsigned int clk_frequency)
    {
        m_io_cfg.cs_gpio_num = cs_gpio_num;
        m_io_cfg.dc_gpio_num = dc_gpio_num;
        m_io_cfg.spi_mode = 0;
        m_io_cfg.pclk_hz = clk_frequency;
        m_io_cfg.trans_queue_depth = 10;
        m_io_cfg.lcd_cmd_bits = 8;
        m_io_cfg.lcd_param_bits = 8;
    }
    void ST7796::set_panel_cfg(gpio_num_t reset_gpio_num)
    {
        m_panel_cfg.reset_gpio_num = reset_gpio_num;
        m_panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
        m_panel_cfg.bits_per_pixel = 16;
    }

    esp_err_t ST7796::spi_init()
    {
        auto err = spi_bus_initialize(m_spi_host, &m_bus_cfg, SPI_DMA_CH_AUTO);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Initialize SPI bus failed.");
            return err;
        }
        return ESP_OK;
    }
    esp_err_t ST7796::io_init()
    {
        auto err = esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(m_spi_host),
                                            &m_io_cfg,
                                            &m_io_handle);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Initialize LCD panel IO failed.");
            return err;
        }
        return ESP_OK;
    }
    esp_err_t ST7796::panel_init()
    {
        if (m_io_handle == nullptr) [[unlikely]]
        {
            ESP_LOGW(TAG, "IO_handle is not initialized.");
            return ESP_FAIL;
        }

        esp_err_t err;
        err = esp_lcd_new_panel_st7796(m_io_handle, &m_panel_cfg, &m_panel_handle);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Create LCD panel failed.");
            return err;
        }

        err = esp_lcd_panel_reset(m_panel_handle);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Reset LCD panel failed.");
            return err;
        }

        err = esp_lcd_panel_init(m_panel_handle);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Initialize LCD panel failed.");
            return err;
        }

        err = esp_lcd_panel_disp_on_off(m_panel_handle, true);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Turn on LCD panel failed.");
            return err;
        }
        return ESP_OK;
    }

    esp_lcd_panel_io_handle_t ST7796::io_handle()
    {
        return m_io_handle;
    }
    esp_lcd_panel_handle_t ST7796::panel_handle()
    {
        return m_panel_handle;
    }

} /* namespace LCD */
