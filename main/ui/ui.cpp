#include "ui.hpp"
#include "lvgl.h"

static lv_obj_t *create_card(lv_obj_t *parent, const char *title, const char *value,
                             lv_color_t color, int x, int y)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 220, 100);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1a1a2e), LV_PART_MAIN);
    lv_obj_set_style_radius(card, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);

    lv_obj_t *title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 10, 10);

    lv_obj_t *val_label = lv_label_create(card);
    lv_label_set_text(val_label, value);
    lv_obj_set_style_text_color(val_label, color, LV_PART_MAIN);
    lv_obj_set_style_text_font(val_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(val_label, LV_ALIGN_BOTTOM_LEFT, 10, -10);

    return card;
}

lv_obj_t *ui_create(lv_obj_t *parent)
{
    if (!parent) parent = lv_scr_act();

    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0f0f23), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Smart Incubator");
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 10);

    create_card(parent, "Temperature", "37.5 C", lv_color_hex(0xff4444), 10, 50);
    create_card(parent, "Humidity", "65 %", lv_color_hex(0x4444ff), 240, 50);

    lv_obj_t *slider = lv_slider_create(parent);
    lv_slider_set_range(slider, 20, 42);
    lv_slider_set_value(slider, 37, LV_ANIM_OFF);
    lv_obj_set_pos(slider, 20, 180);
    lv_obj_set_size(slider, 300, 20);

    lv_obj_t *min_label = lv_label_create(parent);
    lv_label_set_text(min_label, "20 C");
    lv_obj_set_style_text_color(min_label, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_align_to(min_label, slider, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);

    lv_obj_t *max_label = lv_label_create(parent);
    lv_label_set_text(max_label, "42 C");
    lv_obj_set_style_text_color(max_label, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_align_to(max_label, slider, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 5);

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, 20, 240);
    lv_obj_set_size(btn, 150, 40);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x00aa00), LV_PART_MAIN);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Running");
    lv_obj_center(btn_label);

    return parent;
}
