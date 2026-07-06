#include "esp_err.h"
#include "led_strip.h"
#include "soc/gpio_num.h"
#include <cstdint>

#include "led.hpp"

namespace LED
{
    esp_err_t init(gpio_num_t led)
    {
        // LED strip general initialization, according to your led board design
        led_strip_config_t strip_config = {
            .strip_gpio_num = led,                                       // The GPIO that connected to the LED strip's data line
            .max_leds = 12,                                              // The number of LEDs in the strip,
            .led_model = LED_MODEL_WS2812,                               // LED strip model
            .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color order of the strip: GRB
            .flags = {
                .invert_out = false, // don't invert the output signal
            }};

        // LED strip backend configuration: RMT
        led_strip_rmt_config_t rmt_config = {
            .clk_src = RMT_CLK_SRC_DEFAULT,    // different clock source can lead to different power consumption
            .resolution_hz = 10 * 1000 * 1000, // RMT counter clock frequency
            .mem_block_symbols = 0,            // the memory block size used by the RMT channel
            .flags = {
                .with_dma = false, // Using DMA can improve performance when driving more LEDs
            }};

        return led_strip_new_rmt_device(&strip_config,
                                        &rmt_config,
                                        &m_led_handle);
    }

    void on(uint8_t strength)
    {
        for (int i = 0; i < 12; i++)
        {
            led_strip_set_pixel(m_led_handle, i, strength, strength, strength);
        }
        /* Refresh the strip to send data */
        led_strip_refresh(m_led_handle);
    }
    void off()
    {
        led_strip_clear(m_led_handle);
    }
}