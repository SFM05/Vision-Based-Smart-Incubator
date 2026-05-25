#pragma once

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_panel_io.h"

class LvglPort
{
public:
    static esp_err_t init(
        esp_lcd_panel_handle_t panel,
        esp_lcd_panel_io_handle_t io,
        esp_lcd_touch_handle_t touch,
        uint16_t width,
        uint16_t height
    );
    static void tick();
    static void refresh();
};
