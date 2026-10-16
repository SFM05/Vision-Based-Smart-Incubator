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
#include "gui_guider.h"
#include "events_init.h"



void setup_scr_screen_2(lv_ui *ui)
{
    //Write codes screen_2
    ui->screen_2 = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_2, 480, 320);
    lv_obj_set_scrollbar_mode(ui->screen_2, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_2, lv_color_hex(0x7e7e7e), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_2, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_2_Button_screen2to1
    ui->screen_2_Button_screen2to1 = lv_btn_create(ui->screen_2);
    ui->screen_2_Button_screen2to1_label = lv_label_create(ui->screen_2_Button_screen2to1);
    lv_label_set_text(ui->screen_2_Button_screen2to1_label, "Cam_pre");
    lv_label_set_long_mode(ui->screen_2_Button_screen2to1_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_2_Button_screen2to1_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_2_Button_screen2to1, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_2_Button_screen2to1_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_2_Button_screen2to1, 320, 270);
    lv_obj_set_size(ui->screen_2_Button_screen2to1, 160, 50);
    lv_obj_add_flag(ui->screen_2_Button_screen2to1, LV_OBJ_FLAG_CLICKABLE);

    //Write style for screen_2_Button_screen2to1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_2_Button_screen2to1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_2_Button_screen2to1, lv_color_hex(0x6a6a6a), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_2_Button_screen2to1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_2_Button_screen2to1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_2_Button_screen2to1, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_2_Button_screen2to1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_2_Button_screen2to1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_2_Button_screen2to1, &lv_font_montserrat_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_2_Button_screen2to1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_2_Button_screen2to1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    // //Write codes screen_2_Temperature_2
    // ui->screen_2_Temperature_2 = lv_label_create(ui->screen_2);
    // lv_label_set_text(ui->screen_2_Temperature_2, "Temperature: 30.0");
    // lv_label_set_long_mode(ui->screen_2_Temperature_2, LV_LABEL_LONG_WRAP);
    // lv_obj_set_pos(ui->screen_2_Temperature_2, 5, 4);
    // lv_obj_set_size(ui->screen_2_Temperature_2, 250, 25);

    // //Write style for screen_2_Temperature_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    // lv_obj_set_style_border_width(ui->screen_2_Temperature_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_radius(ui->screen_2_Temperature_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_color(ui->screen_2_Temperature_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_font(ui->screen_2_Temperature_2, &lv_font_montserrat_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_opa(ui->screen_2_Temperature_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_letter_space(ui->screen_2_Temperature_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_line_space(ui->screen_2_Temperature_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_text_align(ui->screen_2_Temperature_2, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_opa(ui->screen_2_Temperature_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_pad_top(ui->screen_2_Temperature_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_pad_right(ui->screen_2_Temperature_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_pad_bottom(ui->screen_2_Temperature_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_pad_left(ui->screen_2_Temperature_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_shadow_width(ui->screen_2_Temperature_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_2_Interval_time
    ui->screen_2_Interval_time = lv_label_create(ui->screen_2);
    lv_label_set_text(ui->screen_2_Interval_time, "Interval time");
    lv_label_set_long_mode(ui->screen_2_Interval_time, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_2_Interval_time, 5, 60);
    lv_obj_set_size(ui->screen_2_Interval_time, 180, 27);

    //Write style for screen_2_Interval_time, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_2_Interval_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_2_Interval_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_2_Interval_time, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_2_Interval_time, &lv_font_montserrat_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_2_Interval_time, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_2_Interval_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_2_Interval_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_2_Interval_time, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_2_Interval_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_2_Interval_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_2_Interval_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_2_Interval_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_2_Interval_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_2_Interval_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_2_Count
    ui->screen_2_Count = lv_label_create(ui->screen_2);
    lv_label_set_text(ui->screen_2_Count, "Count:");
    lv_label_set_long_mode(ui->screen_2_Count, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_2_Count, 5, 85);
    lv_obj_set_size(ui->screen_2_Count, 162, 25);

    //Write style for screen_2_Count, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_2_Count, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_2_Count, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_2_Count, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_2_Count, &lv_font_montserrat_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_2_Count, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_2_Count, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_2_Count, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_2_Count, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_2_Count, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_2_Count, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_2_Count, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_2_Count, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_2_Count, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_2_Count, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    // //Write codes screen_2_Temperature_slider
    // ui->screen_2_Temperature_slider = lv_slider_create(ui->screen_2);
    // lv_slider_set_range(ui->screen_2_Temperature_slider, 0, 100);
    // lv_slider_set_mode(ui->screen_2_Temperature_slider, LV_SLIDER_MODE_NORMAL);
    // lv_slider_set_value(ui->screen_2_Temperature_slider, 17, LV_ANIM_OFF);
    // lv_obj_set_pos(ui->screen_2_Temperature_slider, 30, 34);
    // lv_obj_set_size(ui->screen_2_Temperature_slider, 400, 20);

    // //Write style for screen_2_Temperature_slider, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    // lv_obj_set_style_bg_opa(ui->screen_2_Temperature_slider, 60, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_color(ui->screen_2_Temperature_slider, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_grad_dir(ui->screen_2_Temperature_slider, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_radius(ui->screen_2_Temperature_slider, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_outline_width(ui->screen_2_Temperature_slider, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_shadow_width(ui->screen_2_Temperature_slider, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    // //Write style for screen_2_Temperature_slider, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    // lv_obj_set_style_bg_opa(ui->screen_2_Temperature_slider, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_color(ui->screen_2_Temperature_slider, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_grad_dir(ui->screen_2_Temperature_slider, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    // lv_obj_set_style_radius(ui->screen_2_Temperature_slider, 8, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    // //Write style for screen_2_Temperature_slider, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    // lv_obj_set_style_bg_opa(ui->screen_2_Temperature_slider, 255, LV_PART_KNOB|LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_color(ui->screen_2_Temperature_slider, lv_color_hex(0x2195f6), LV_PART_KNOB|LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_grad_dir(ui->screen_2_Temperature_slider, LV_GRAD_DIR_NONE, LV_PART_KNOB|LV_STATE_DEFAULT);
    // lv_obj_set_style_radius(ui->screen_2_Temperature_slider, 8, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes screen_2_Count_interval
    ui->screen_2_Count_interval = lv_dropdown_create(ui->screen_2);
    lv_dropdown_set_options(ui->screen_2_Count_interval, "30s\n10min\n30min");
    lv_obj_set_pos(ui->screen_2_Count_interval, 10, 120);
    lv_obj_set_size(ui->screen_2_Count_interval, 150, 30);

    //Write style for screen_2_Count_interval, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->screen_2_Count_interval, lv_color_hex(0x0D3055), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_2_Count_interval, &lv_font_montserrat_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_2_Count_interval, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_2_Count_interval, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_2_Count_interval, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_2_Count_interval, lv_color_hex(0xe1e6ee), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_2_Count_interval, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_2_Count_interval, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_2_Count_interval, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_2_Count_interval, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_2_Count_interval, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_2_Count_interval, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_2_Count_interval, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_2_Count_interval, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_2_Count_interval, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_CHECKED for &style_screen_2_Count_interval_extra_list_selected_checked
    static lv_style_t style_screen_2_Count_interval_extra_list_selected_checked;
    ui_init_style(&style_screen_2_Count_interval_extra_list_selected_checked);

    lv_style_set_border_width(&style_screen_2_Count_interval_extra_list_selected_checked, 1);
    lv_style_set_border_opa(&style_screen_2_Count_interval_extra_list_selected_checked, 255);
    lv_style_set_border_color(&style_screen_2_Count_interval_extra_list_selected_checked, lv_color_hex(0xe1e6ee));
    lv_style_set_border_side(&style_screen_2_Count_interval_extra_list_selected_checked, LV_BORDER_SIDE_FULL);
    lv_style_set_radius(&style_screen_2_Count_interval_extra_list_selected_checked, 3);
    lv_style_set_bg_opa(&style_screen_2_Count_interval_extra_list_selected_checked, 255);
    lv_style_set_bg_color(&style_screen_2_Count_interval_extra_list_selected_checked, lv_color_hex(0x00a1b5));
    lv_style_set_bg_grad_dir(&style_screen_2_Count_interval_extra_list_selected_checked, LV_GRAD_DIR_NONE);
    lv_obj_add_style(lv_dropdown_get_list(ui->screen_2_Count_interval), &style_screen_2_Count_interval_extra_list_selected_checked, LV_PART_SELECTED|LV_STATE_CHECKED);

    //Write style state: LV_STATE_DEFAULT for &style_screen_2_Count_interval_extra_list_main_default
    static lv_style_t style_screen_2_Count_interval_extra_list_main_default;
    ui_init_style(&style_screen_2_Count_interval_extra_list_main_default);

    lv_style_set_max_height(&style_screen_2_Count_interval_extra_list_main_default, 90);
    lv_style_set_text_color(&style_screen_2_Count_interval_extra_list_main_default, lv_color_hex(0x0D3055));
    lv_style_set_text_font(&style_screen_2_Count_interval_extra_list_main_default, &lv_font_montserrat_12);
    lv_style_set_text_opa(&style_screen_2_Count_interval_extra_list_main_default, 255);
    lv_style_set_border_width(&style_screen_2_Count_interval_extra_list_main_default, 1);
    lv_style_set_border_opa(&style_screen_2_Count_interval_extra_list_main_default, 255);
    lv_style_set_border_color(&style_screen_2_Count_interval_extra_list_main_default, lv_color_hex(0xe1e6ee));
    lv_style_set_border_side(&style_screen_2_Count_interval_extra_list_main_default, LV_BORDER_SIDE_FULL);
    lv_style_set_radius(&style_screen_2_Count_interval_extra_list_main_default, 3);
    lv_style_set_bg_opa(&style_screen_2_Count_interval_extra_list_main_default, 255);
    lv_style_set_bg_color(&style_screen_2_Count_interval_extra_list_main_default, lv_color_hex(0xffffff));
    lv_style_set_bg_grad_dir(&style_screen_2_Count_interval_extra_list_main_default, LV_GRAD_DIR_NONE);
    lv_obj_add_style(lv_dropdown_get_list(ui->screen_2_Count_interval), &style_screen_2_Count_interval_extra_list_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_DEFAULT for &style_screen_2_Count_interval_extra_list_scrollbar_default
    static lv_style_t style_screen_2_Count_interval_extra_list_scrollbar_default;
    ui_init_style(&style_screen_2_Count_interval_extra_list_scrollbar_default);

    lv_style_set_radius(&style_screen_2_Count_interval_extra_list_scrollbar_default, 3);
    lv_style_set_bg_opa(&style_screen_2_Count_interval_extra_list_scrollbar_default, 255);
    lv_style_set_bg_color(&style_screen_2_Count_interval_extra_list_scrollbar_default, lv_color_hex(0x00ff00));
    lv_style_set_bg_grad_dir(&style_screen_2_Count_interval_extra_list_scrollbar_default, LV_GRAD_DIR_NONE);
    lv_obj_add_style(lv_dropdown_get_list(ui->screen_2_Count_interval), &style_screen_2_Count_interval_extra_list_scrollbar_default, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);

    //Write codes screen_2_Upload
    ui->screen_2_Upload = lv_label_create(ui->screen_2);
    lv_label_set_text(ui->screen_2_Upload, "Upload:");
    lv_label_set_long_mode(ui->screen_2_Upload, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_2_Upload, 244, 85);
    lv_obj_set_size(ui->screen_2_Upload, 162, 25);

    //Write style for screen_2_Upload, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_2_Upload, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_2_Upload, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_2_Upload, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_2_Upload, &lv_font_montserrat_24, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_2_Upload, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_2_Upload, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_2_Upload, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_2_Upload, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_2_Upload, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_2_Upload, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_2_Upload, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_2_Upload, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_2_Upload, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_2_Upload, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_2_Upload_interval
    ui->screen_2_Upload_interval = lv_dropdown_create(ui->screen_2);
    lv_dropdown_set_options(ui->screen_2_Upload_interval, "1s\n15s\n30s");
    lv_obj_set_pos(ui->screen_2_Upload_interval, 250, 120);
    lv_obj_set_size(ui->screen_2_Upload_interval, 150, 30);

    //Write style for screen_2_Upload_interval, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_text_color(ui->screen_2_Upload_interval, lv_color_hex(0x0D3055), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_2_Upload_interval, &lv_font_montserrat_12, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_2_Upload_interval, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_2_Upload_interval, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_2_Upload_interval, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_2_Upload_interval, lv_color_hex(0xe1e6ee), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_2_Upload_interval, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_2_Upload_interval, 8, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_2_Upload_interval, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_2_Upload_interval, 6, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_2_Upload_interval, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_2_Upload_interval, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_2_Upload_interval, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_2_Upload_interval, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_2_Upload_interval, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_CHECKED for &style_screen_2_Upload_interval_extra_list_selected_checked
    static lv_style_t style_screen_2_Upload_interval_extra_list_selected_checked;
    ui_init_style(&style_screen_2_Upload_interval_extra_list_selected_checked);

    lv_style_set_border_width(&style_screen_2_Upload_interval_extra_list_selected_checked, 1);
    lv_style_set_border_opa(&style_screen_2_Upload_interval_extra_list_selected_checked, 255);
    lv_style_set_border_color(&style_screen_2_Upload_interval_extra_list_selected_checked, lv_color_hex(0xe1e6ee));
    lv_style_set_border_side(&style_screen_2_Upload_interval_extra_list_selected_checked, LV_BORDER_SIDE_FULL);
    lv_style_set_radius(&style_screen_2_Upload_interval_extra_list_selected_checked, 3);
    lv_style_set_bg_opa(&style_screen_2_Upload_interval_extra_list_selected_checked, 255);
    lv_style_set_bg_color(&style_screen_2_Upload_interval_extra_list_selected_checked, lv_color_hex(0x00a1b5));
    lv_style_set_bg_grad_dir(&style_screen_2_Upload_interval_extra_list_selected_checked, LV_GRAD_DIR_NONE);
    lv_obj_add_style(lv_dropdown_get_list(ui->screen_2_Upload_interval), &style_screen_2_Upload_interval_extra_list_selected_checked, LV_PART_SELECTED|LV_STATE_CHECKED);

    //Write style state: LV_STATE_DEFAULT for &style_screen_2_Upload_interval_extra_list_main_default
    static lv_style_t style_screen_2_Upload_interval_extra_list_main_default;
    ui_init_style(&style_screen_2_Upload_interval_extra_list_main_default);

    lv_style_set_max_height(&style_screen_2_Upload_interval_extra_list_main_default, 90);
    lv_style_set_text_color(&style_screen_2_Upload_interval_extra_list_main_default, lv_color_hex(0x0D3055));
    lv_style_set_text_font(&style_screen_2_Upload_interval_extra_list_main_default, &lv_font_montserrat_12);
    lv_style_set_text_opa(&style_screen_2_Upload_interval_extra_list_main_default, 255);
    lv_style_set_border_width(&style_screen_2_Upload_interval_extra_list_main_default, 1);
    lv_style_set_border_opa(&style_screen_2_Upload_interval_extra_list_main_default, 255);
    lv_style_set_border_color(&style_screen_2_Upload_interval_extra_list_main_default, lv_color_hex(0xe1e6ee));
    lv_style_set_border_side(&style_screen_2_Upload_interval_extra_list_main_default, LV_BORDER_SIDE_FULL);
    lv_style_set_radius(&style_screen_2_Upload_interval_extra_list_main_default, 3);
    lv_style_set_bg_opa(&style_screen_2_Upload_interval_extra_list_main_default, 255);
    lv_style_set_bg_color(&style_screen_2_Upload_interval_extra_list_main_default, lv_color_hex(0xffffff));
    lv_style_set_bg_grad_dir(&style_screen_2_Upload_interval_extra_list_main_default, LV_GRAD_DIR_NONE);
    lv_obj_add_style(lv_dropdown_get_list(ui->screen_2_Upload_interval), &style_screen_2_Upload_interval_extra_list_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_DEFAULT for &style_screen_2_Upload_interval_extra_list_scrollbar_default
    static lv_style_t style_screen_2_Upload_interval_extra_list_scrollbar_default;
    ui_init_style(&style_screen_2_Upload_interval_extra_list_scrollbar_default);

    lv_style_set_radius(&style_screen_2_Upload_interval_extra_list_scrollbar_default, 3);
    lv_style_set_bg_opa(&style_screen_2_Upload_interval_extra_list_scrollbar_default, 255);
    lv_style_set_bg_color(&style_screen_2_Upload_interval_extra_list_scrollbar_default, lv_color_hex(0x00ff00));
    lv_style_set_bg_grad_dir(&style_screen_2_Upload_interval_extra_list_scrollbar_default, LV_GRAD_DIR_NONE);
    lv_obj_add_style(lv_dropdown_get_list(ui->screen_2_Upload_interval), &style_screen_2_Upload_interval_extra_list_scrollbar_default, LV_PART_SCROLLBAR|LV_STATE_DEFAULT);

    //The custom code of screen_2.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen_2);

    //Init events for screen.
    events_init_screen_2(ui);
}
