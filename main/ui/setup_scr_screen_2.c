#include "gui_guider.h"
#include "events_init.h"

void setup_scr_screen_2(lv_ui *ui)
{
    if (ui->screen_2_del)
    {
        ui->screen_2 = lv_obj_create(NULL);
        lv_obj_set_size(ui->screen_2, 480, 320);
        lv_obj_set_scrollbar_mode(ui->screen_2, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_opa(ui->screen_2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui->screen_2, lv_color_hex(0x7e7e7e), LV_PART_MAIN | LV_STATE_DEFAULT);

        ui->screen_2_label_1 = lv_label_create(ui->screen_2);
        lv_label_set_text(ui->screen_2_label_1, "Temperature");
        lv_obj_set_pos(ui->screen_2_label_1, 30, 25);
        lv_obj_set_size(ui->screen_2_label_1, 420, 30);
        lv_obj_set_style_text_color(ui->screen_2_label_1, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui->screen_2_label_1, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui->screen_2_slider_2 = lv_slider_create(ui->screen_2);
        lv_slider_set_range(ui->screen_2_slider_2, 0, 25);
        lv_slider_set_mode(ui->screen_2_slider_2, LV_SLIDER_MODE_NORMAL);
        lv_slider_set_value(ui->screen_2_slider_2, 10, LV_ANIM_OFF);
        lv_obj_set_pos(ui->screen_2_slider_2, 40, 60);
        lv_obj_set_size(ui->screen_2_slider_2, 400, 20);
        lv_obj_set_style_bg_opa(ui->screen_2_slider_2, 60, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ui->screen_2_slider_2, lv_color_hex(0x2195f6), LV_PART_MAIN);
        lv_obj_set_style_radius(ui->screen_2_slider_2, 8, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui->screen_2_slider_2, 255, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(ui->screen_2_slider_2, lv_color_hex(0x2195f6), LV_PART_INDICATOR);
        lv_obj_set_style_radius(ui->screen_2_slider_2, 8, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(ui->screen_2_slider_2, 255, LV_PART_KNOB);
        lv_obj_set_style_bg_color(ui->screen_2_slider_2, lv_color_hex(0x2195f6), LV_PART_KNOB);
        lv_obj_set_style_radius(ui->screen_2_slider_2, 8, LV_PART_KNOB);

        ui->screen_2_label_2 = lv_label_create(ui->screen_2);
        lv_label_set_text(ui->screen_2_label_2, "Humidity");
        lv_obj_set_pos(ui->screen_2_label_2, 30, 90);
        lv_obj_set_size(ui->screen_2_label_2, 190, 30);
        lv_obj_set_style_text_color(ui->screen_2_label_2, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui->screen_2_label_2, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui->screen_2_label_4 = lv_label_create(ui->screen_2);
        lv_label_set_text(ui->screen_2_label_4, "CO2");
        lv_obj_set_pos(ui->screen_2_label_4, 240, 90);
        lv_obj_set_size(ui->screen_2_label_4, 200, 30);
        lv_obj_set_style_text_color(ui->screen_2_label_4, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui->screen_2_label_4, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui->screen_2_label_3 = lv_label_create(ui->screen_2);
        lv_label_set_text(ui->screen_2_label_3, "Interval time");
        lv_label_set_long_mode(ui->screen_2_label_3, LV_LABEL_LONG_WRAP);
        lv_obj_set_pos(ui->screen_2_label_3, 30, 145);
        lv_obj_set_size(ui->screen_2_label_3, 220, 30);
        lv_obj_set_style_text_color(ui->screen_2_label_3, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui->screen_2_label_3, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui->screen_2_slider_1 = lv_slider_create(ui->screen_2);
        lv_slider_set_range(ui->screen_2_slider_1, 0, 5);
        lv_slider_set_mode(ui->screen_2_slider_1, LV_SLIDER_MODE_NORMAL);
        lv_slider_set_value(ui->screen_2_slider_1, 2, LV_ANIM_OFF);
        lv_obj_set_pos(ui->screen_2_slider_1, 40, 185);
        lv_obj_set_size(ui->screen_2_slider_1, 400, 20);
        lv_obj_set_style_bg_opa(ui->screen_2_slider_1, 60, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ui->screen_2_slider_1, lv_color_hex(0x2195f6), LV_PART_MAIN);
        lv_obj_set_style_radius(ui->screen_2_slider_1, 8, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui->screen_2_slider_1, 255, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(ui->screen_2_slider_1, lv_color_hex(0x2195f6), LV_PART_INDICATOR);
        lv_obj_set_style_radius(ui->screen_2_slider_1, 8, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(ui->screen_2_slider_1, 255, LV_PART_KNOB);
        lv_obj_set_style_bg_color(ui->screen_2_slider_1, lv_color_hex(0x2195f6), LV_PART_KNOB);
        lv_obj_set_style_radius(ui->screen_2_slider_1, 8, LV_PART_KNOB);

        ui->screen_2_btn_1 = lv_btn_create(ui->screen_2);
        lv_obj_set_pos(ui->screen_2_btn_1, 320, 265);
        lv_obj_set_size(ui->screen_2_btn_1, 160, 50);
        lv_obj_set_style_bg_color(ui->screen_2_btn_1, lv_color_hex(0x6a6a6a), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(ui->screen_2_btn_1, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(ui->screen_2_btn_1, 0, 0);

        ui->screen_2_btn_1_label = lv_label_create(ui->screen_2_btn_1);
        lv_label_set_text(ui->screen_2_btn_1_label, "Cam_pre");
        lv_obj_set_style_text_color(ui->screen_2_btn_1_label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui->screen_2_btn_1_label, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_center(ui->screen_2_btn_1_label);

        events_init_screen_2(ui);
        ui->screen_2_del = false;
    }
    lv_obj_update_layout(ui->screen_2);
}
