#include "led_ring.hpp"
#include "led_strip.h"
#include "esp_log.h"

static const char *TAG = "LED_RING";

static led_strip_handle_t s_led = NULL;
static uint16_t s_num_leds = 0;

esp_err_t led_ring_init(gpio_num_t gpio, uint16_t num_leds)
{
    if (s_led) return ESP_OK;

    led_strip_config_t cfg = {
        .strip_gpio_num = (int)gpio,
        .max_leds = num_leds,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    led_strip_rmt_config_t rmt = {};
    rmt.clk_src = RMT_CLK_SRC_DEFAULT;
    rmt.resolution_hz = 10 * 1000 * 1000;
    esp_err_t err = led_strip_new_rmt_device(&cfg, &rmt, &s_led);
    if (err == ESP_OK) {
        s_num_leds = num_leds;
        ESP_LOGI(TAG, "LED ring ready (GPIO%d, %d LEDs)", gpio, num_leds);
    }
    return err;
}

esp_err_t led_ring_set_all(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_led) return ESP_ERR_INVALID_STATE;
    for (uint16_t i = 0; i < s_num_leds; i++) {
        led_strip_set_pixel(s_led, i, r, g, b);
    }
    return led_strip_refresh(s_led);
}

esp_err_t led_ring_on(void)
{
    return led_ring_set_all(255, 255, 255);
}

esp_err_t led_ring_off(void)
{
    return led_ring_set_all(0, 0, 0);
}

esp_err_t led_ring_deinit(void)
{
    if (s_led) {
        led_ring_off();
        led_strip_del(s_led);
        s_led = NULL;
        s_num_leds = 0;
    }
    return ESP_OK;
}
