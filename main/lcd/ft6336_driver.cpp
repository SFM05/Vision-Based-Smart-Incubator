#include "esp_log.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_ft5x06.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

#include "ft6336_driver.hpp"

/**
 * @brief 手动复位 FT6336，比库默认的 10ms 更稳妥
 */
static void manual_reset(gpio_num_t rst_gpio, int active_level, int wait_ms)
{
    gpio_config_t cfg = {
        .pin_bit_mask = BIT64(rst_gpio),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg);
    gpio_set_level(rst_gpio, active_level);
    vTaskDelay(pdMS_TO_TICKS(wait_ms));
    gpio_set_level(rst_gpio, !active_level);
    vTaskDelay(pdMS_TO_TICKS(wait_ms));
}

static const char *TAG = "FT6336";

namespace LCD
{
    FT6336::FT6336(uint16_t width,
                   uint16_t height) : m_width{width},
                                      m_height{height},
                                      m_i2c_cfg{},
                                      m_i2c_handle{},
                                      m_io_cfg{},
                                      m_io_handle{},
                                      m_touch_cfg{},
                                      m_touch_handle{}
    {
        m_io_cfg.scl_speed_hz = 20000;
        m_io_cfg.dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS;
        m_io_cfg.control_phase_bytes = 1;
        m_io_cfg.dc_bit_offset = 0;
        m_io_cfg.lcd_cmd_bits = 8;
        m_io_cfg.flags.disable_control_phase = 1;
    }

    void FT6336::set_i2c_config(gpio_num_t sda_io_num, gpio_num_t scl_io_num)
    {
        m_i2c_cfg.i2c_port = I2C_NUM_1;
        m_i2c_cfg.sda_io_num = sda_io_num;
        m_i2c_cfg.scl_io_num = scl_io_num;
        m_i2c_cfg.clk_source = I2C_CLK_SRC_XTAL;
        m_i2c_cfg.glitch_ignore_cnt = 7;
        m_i2c_cfg.flags.enable_internal_pullup = false;
    }
    void FT6336::set_touch_config(gpio_num_t rst_io_num,
                                  gpio_num_t int_io_num,
                                  esp_lcd_touch_interrupt_callback_t iqr_handle)
    {
        m_touch_cfg.x_max = m_height;
        m_touch_cfg.y_max = m_width;
        m_touch_cfg.rst_gpio_num = rst_io_num;
        m_touch_cfg.int_gpio_num = int_io_num;

        m_touch_cfg.levels.interrupt = 0;
        m_touch_cfg.levels.reset = 0;
        m_touch_cfg.interrupt_callback = iqr_handle;

        m_touch_cfg.flags.swap_xy = 0;
        m_touch_cfg.flags.mirror_x = 0;
        m_touch_cfg.flags.mirror_y = 0;
    }

    esp_err_t FT6336::i2c_init()
    {
        auto err = i2c_new_master_bus(&m_i2c_cfg, &m_i2c_handle);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Initialize I2C bus failed.");
            return err;
        }
        return ESP_OK;
    }
    esp_err_t FT6336::io_init()
    {
        esp_err_t err;
        err = esp_lcd_new_panel_io_i2c(m_i2c_handle, &m_io_cfg, &m_io_handle);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Initialize LCD panel IO failed.");
            return err;
        }
        return ESP_OK;
    }
    esp_err_t FT6336::touch_init()
    {
        /**
         * @brief 用自己的复位（100ms）取代库默认的 10ms
         */
        gpio_num_t rst_save = m_touch_cfg.rst_gpio_num;
        if (rst_save != GPIO_NUM_NC) {
            manual_reset(rst_save, m_touch_cfg.levels.reset, 100);
            m_touch_cfg.rst_gpio_num = GPIO_NUM_NC;
        }

        esp_err_t err;
        err = esp_lcd_touch_new_i2c_ft5x06(m_io_handle, &m_touch_cfg, &m_touch_handle);

        if (rst_save != GPIO_NUM_NC) {
            m_touch_cfg.rst_gpio_num = rst_save;
        }
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Initialize touch driver failed.");
            return err;
        }

        /**
     * @brief 中断引脚需要上拉，否则会一直唤起中断
     */
        err = gpio_pullup_en(m_touch_cfg.int_gpio_num);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Set pull mode for interrupt pin failed.");
            return err;
        }
        

        return ESP_OK;
    }

    i2c_master_bus_handle_t FT6336::i2c_handle() const
    {
        return m_i2c_handle;
    }
    esp_lcd_panel_io_handle_t FT6336::io_handle() const
    {
        return m_io_handle;
    }
    esp_lcd_touch_handle_t FT6336::touch_handle() const
    {
        return m_touch_handle;
    }
} /* namespace LCD */
