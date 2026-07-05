#pragma once

#include "dl_detect_define.hpp"
#include "dl_image_define.hpp"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include <vector>

esp_err_t init(esp_lcd_panel_handle_t panel,
               esp_lcd_panel_io_handle_t io,
               esp_lcd_touch_handle_t touch,
               uint16_t width,
               uint16_t height);

struct image_display_process_args
{
    std::vector<dl::detect::result_t> &result;
    dl::image::img_t src_img;
    dl::image::img_t dest_buffer;
    SemaphoreHandle_t &flush_screen;
};
void image_display_process(void *argv);