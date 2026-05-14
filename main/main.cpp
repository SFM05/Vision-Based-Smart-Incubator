#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "st7796.hpp"
#include "ft6336.hpp"
#include "lvgl.h"
#include "lv_port_disp.hpp"
#include "lv_port_indev.hpp"
#include "camera.hpp"

static const char *TAG = "main";

static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(1);
}

extern "C" void app_main(void)
{
    st7796_init();
    ft6336_init();
    camera_init();

    lv_init();

    /* LVGL tick — required for timers (indev read, display refresh) to fire */
    const esp_timer_create_args_t tick_timer_args = {
        .callback = &lvgl_tick_cb,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 1000));

    lv_port_disp_init();
    lv_port_indev_init();

    ESP_LOGI(TAG, "LVGL + direct camera");

    /* Direct camera region (right side, clear of LVGL area) */
    int ox = ST7796_LCD_W - CAM_DISP_W;
    int oy = (ST7796_LCD_H - CAM_DISP_H) / 2;

    /* LVGL UI sliders (left side, no canvas) */
    lv_obj_t * slider1 = lv_slider_create(lv_scr_act());
    lv_obj_set_size(slider1, 28, 160);
    lv_obj_align(slider1, LV_ALIGN_LEFT_MID, 18, 0);
    lv_slider_set_range(slider1, 0, 100);
    lv_slider_set_value(slider1, 50, LV_ANIM_OFF);

    lv_obj_t * slider2 = lv_slider_create(lv_scr_act());
    lv_obj_set_size(slider2, 28, 160);
    lv_obj_align(slider2, LV_ALIGN_LEFT_MID, 72, 0);
    lv_slider_set_range(slider2, 0, 100);
    lv_slider_set_value(slider2, 50, LV_ANIM_OFF);

    st7796_fill_screen(0x0000);

    int frame_no = 0;
    int64_t last_log = esp_timer_get_time();
    while (1) {
        lv_timer_handler();
        if (cam_frame_ready) {
            cam_frame_ready = false;
            st7796_flush(ox, oy, ox + CAM_DISP_W - 1, oy + CAM_DISP_H - 1, cam_fb);
            frame_no++;
        }
        int64_t now = esp_timer_get_time();
        if (now - last_log > 5000000) {
            ESP_LOGI(TAG, "fps=%d", frame_no * 1000000 / (now - last_log));
            last_log = now;
            frame_no = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
