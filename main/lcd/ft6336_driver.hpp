#include "esp_lcd_touch.h"
#include "esp_lcd_touch_ft5x06.h"
#include "driver/i2c_master.h"

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
        FT6336(uint16_t width, uint16_t height);
        void set_i2c_config(gpio_num_t sda_io_num, gpio_num_t scl_io_num);
        void set_touch_config(gpio_num_t rst_io_num,
                              gpio_num_t int_io_num,
                              esp_lcd_touch_interrupt_callback_t iqr_handle = nullptr);

        esp_err_t i2c_init();
        esp_err_t io_init();
        esp_err_t touch_init();

        i2c_master_bus_handle_t i2c_handle() const;
        esp_lcd_panel_io_handle_t io_handle() const;
        esp_lcd_touch_handle_t touch_handle() const;
    };
} // namespace LCD
