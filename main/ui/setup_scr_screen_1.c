/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui_guider.h"
#include "events_init.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"


void setup_scr_screen_1(lv_ui *ui)
{
    //Write codes screen_1
    ui->screen_1 = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_1, 480, 320);
    lv_obj_set_scrollbar_mode(ui->screen_1, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_1, lv_color_hex(0x7e7e7e), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_1_canvas
    ui->screen_1_canvas = lv_canvas_create(ui->screen_1);
    static void *canvas_buf = NULL;
    if (!canvas_buf) {
        size_t sz = LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(320, 320);
        canvas_buf = heap_caps_aligned_alloc(128, sz, MALLOC_CAP_DEFAULT);
        if (canvas_buf) memset(canvas_buf, 0, sz);
    }
    lv_canvas_set_buffer(ui->screen_1_canvas, canvas_buf, 320, 320, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_canvas_fill_bg(ui->screen_1_canvas, lv_color_hex(0xffffff), 255);
    if (canvas_buf) {
        size_t sz = LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(320, 320);
        esp_cache_msync(canvas_buf, sz, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    }
    //Canvas draw rectangle
    lv_draw_rect_dsc_t screen_1_canvas_rect_dsc_0;
    lv_draw_rect_dsc_init(&screen_1_canvas_rect_dsc_0);
    screen_1_canvas_rect_dsc_0.radius = 0;
    screen_1_canvas_rect_dsc_0.bg_opa = 255;
    screen_1_canvas_rect_dsc_0.bg_color = lv_color_hex(0x0775B7);
    screen_1_canvas_rect_dsc_0.bg_grad.dir = LV_GRAD_DIR_NONE;
    screen_1_canvas_rect_dsc_0.border_width = 0;
    screen_1_canvas_rect_dsc_0.border_opa = 255;
    screen_1_canvas_rect_dsc_0.border_color = lv_color_hex(0x000000);
    lv_canvas_draw_rect(ui->screen_1_canvas, 100, 80, 100, 50, &screen_1_canvas_rect_dsc_0);

    lv_obj_set_pos(ui->screen_1_canvas, 0, 0);
    lv_obj_set_size(ui->screen_1_canvas, 320, 320);
    lv_obj_set_scrollbar_mode(ui->screen_1_canvas, LV_SCROLLBAR_MODE_OFF);

    //Write codes screen_1_Num
    ui->screen_1_Num = lv_label_create(ui->screen_1);
    lv_label_set_text(ui->screen_1_Num, "Num:");
    lv_label_set_long_mode(ui->screen_1_Num, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_1_Num, 325, 0);
    lv_obj_set_size(ui->screen_1_Num, 155, 50);

    //Write style for screen_1_Num, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_1_Num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_1_Num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_1_Num, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_1_Num, &lv_font_montserrat_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_1_Num, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_1_Num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_1_Num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_1_Num, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_1_Num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_1_Num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_1_Num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_1_Num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_1_Num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_1_Num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_1_Button_screen1to2
    ui->screen_1_Button_screen1to2 = lv_btn_create(ui->screen_1);
    ui->screen_1_Button_screen1to2_label = lv_label_create(ui->screen_1_Button_screen1to2);
    lv_label_set_text(ui->screen_1_Button_screen1to2_label, "Parameter");
    lv_label_set_long_mode(ui->screen_1_Button_screen1to2_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_1_Button_screen1to2_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_1_Button_screen1to2, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_1_Button_screen1to2_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_1_Button_screen1to2, 320, 270);
    lv_obj_set_size(ui->screen_1_Button_screen1to2, 160, 50);
    lv_obj_add_flag(ui->screen_1_Button_screen1to2, LV_OBJ_FLAG_CLICKABLE);

    //Write style for screen_1_Button_screen1to2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_1_Button_screen1to2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_1_Button_screen1to2, lv_color_hex(0x6a6a6a), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_1_Button_screen1to2, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_1_Button_screen1to2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_1_Button_screen1to2, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_1_Button_screen1to2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_1_Button_screen1to2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_1_Button_screen1to2, &lv_font_montserrat_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_1_Button_screen1to2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_1_Button_screen1to2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_1_Temperature
    ui->screen_1_Temperature = lv_label_create(ui->screen_1);
    lv_label_set_text(ui->screen_1_Temperature, "Temp:");
    lv_label_set_long_mode(ui->screen_1_Temperature, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_1_Temperature, 325, 50);
    lv_obj_set_size(ui->screen_1_Temperature, 170, 50);

    //Write style for screen_1_Temperature, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_1_Temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_1_Temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_1_Temperature, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_1_Temperature, &lv_font_montserrat_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_1_Temperature, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_1_Temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_1_Temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_1_Temperature, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_1_Temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_1_Temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_1_Temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_1_Temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_1_Temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_1_Temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_1_Humidity
    ui->screen_1_Humidity = lv_label_create(ui->screen_1);
    lv_label_set_text(ui->screen_1_Humidity, "Hum:");
    lv_label_set_long_mode(ui->screen_1_Humidity, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_1_Humidity, 325, 100);
    lv_obj_set_size(ui->screen_1_Humidity, 155, 50);

    //Write style for screen_1_Humidity, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_1_Humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_1_Humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_1_Humidity, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_1_Humidity, &lv_font_montserrat_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_1_Humidity, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_1_Humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_1_Humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_1_Humidity, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_1_Humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_1_Humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_1_Humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_1_Humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_1_Humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_1_Humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    // //Write codes screen_1_CO2
    // ui->screen_1_CO2 = lv_label_create(ui->screen_1);
    // lv_label_set_text(ui->screen_1_CO2, "CO2:");
    // lv_label_set_long_mode(ui->screen_1_CO2, LV_LABEL_LONG_WRAP);
    // lv_obj_set_pos(ui->screen_1_CO2, 325, 150);
    // lv_obj_set_size(ui->screen_1_CO2, 155, 50);

    // //Write style for screen_1_CO2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    // lv_obj_set_style_border_width(ui->screen_1_CO2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_radius(ui->screen_1_CO2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_color(ui->screen_1_CO2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_font(ui->screen_1_CO2, &lv_font_montserrat_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_opa(ui->screen_1_CO2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_letter_space(ui->screen_1_CO2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_line_space(ui->screen_1_CO2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_align(ui->screen_1_CO2, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_opa(ui->screen_1_CO2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_pad_top(ui->screen_1_CO2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_pad_right(ui->screen_1_CO2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_pad_bottom(ui->screen_1_CO2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_pad_left(ui->screen_1_CO2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_shadow_width(ui->screen_1_CO2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    // //Write codes screen_1_Time_until_next
    // ui->screen_1_Time_until_next = lv_label_create(ui->screen_1);
    // lv_label_set_text(ui->screen_1_Time_until_next, "Time until next:");
    // lv_label_set_long_mode(ui->screen_1_Time_until_next, LV_LABEL_LONG_WRAP);
    // lv_obj_set_pos(ui->screen_1_Time_until_next, 325, 200);
    // lv_obj_set_size(ui->screen_1_Time_until_next, 155, 50);

    // //Write style for screen_1_Time_until_next, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    // lv_obj_set_style_border_width(ui->screen_1_Time_until_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_radius(ui->screen_1_Time_until_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_color(ui->screen_1_Time_until_next, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_font(ui->screen_1_Time_until_next, &lv_font_montserrat_16, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_opa(ui->screen_1_Time_until_next, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_letter_space(ui->screen_1_Time_until_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_line_space(ui->screen_1_Time_until_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_align(ui->screen_1_Time_until_next, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_opa(ui->screen_1_Time_until_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_pad_top(ui->screen_1_Time_until_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_pad_right(ui->screen_1_Time_until_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_pad_bottom(ui->screen_1_Time_until_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_pad_left(ui->screen_1_Time_until_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_shadow_width(ui->screen_1_Time_until_next, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_1_led
    ui->screen_1_led = lv_led_create(ui->screen_1);
    lv_led_set_brightness(ui->screen_1_led, 255);
    lv_led_set_color(ui->screen_1_led, lv_color_hex(0x7FFF00));
    lv_obj_set_pos(ui->screen_1_led, 460, 10);
    lv_obj_set_size(ui->screen_1_led, 10, 10);

    //The custom code of screen_1.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen_1);

    //Init events for screen.
    events_init_screen_1(ui);
}
