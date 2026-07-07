#pragma once

#include "dl_detect_define.hpp"
#include "dl_image_define.hpp"
#include "driver/i2c_types.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "freertos/idf_additions.h"
#include <cassert>
#include <vector>

inline SemaphoreHandle_t i2c_mutex;
inline void i2c_mutex_init() { i2c_mutex = xSemaphoreCreateMutex(); }

inline SemaphoreHandle_t lv_handle = NULL;
inline void lv_mutex_init()
{
    lv_handle = xSemaphoreCreateMutex();
    assert(lv_handle);
}
inline void lv_lock() { xSemaphoreTake(lv_handle, portMAX_DELAY); }
inline void lv_unlock() { xSemaphoreGive(lv_handle); }

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

void env_display_process(void *argv);
