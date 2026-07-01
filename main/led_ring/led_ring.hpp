#pragma once

#include "esp_err.h"
#include "driver/gpio.h"

esp_err_t led_ring_init(gpio_num_t gpio, uint16_t num_leds);
esp_err_t led_ring_on(void);
esp_err_t led_ring_off(void);
esp_err_t led_ring_set_all(uint8_t r, uint8_t g, uint8_t b);
esp_err_t led_ring_deinit(void);
