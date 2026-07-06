#pragma once

#include "esp_err.h"
#include "led_strip.h"
#include "soc/gpio_num.h"
#include <cstdint>

namespace LED
{
    inline led_strip_handle_t m_led_handle;
    esp_err_t init(gpio_num_t led);
    void on(uint8_t strength);
    void off();
}