#include "gui_guider.h"

void setup_scr_screen(lv_ui *ui)
{
    if (ui->screen_del)
    {
        ui->screen = lv_obj_create(NULL);
        lv_obj_set_size(ui->screen, 480, 320);
        lv_obj_set_scrollbar_mode(ui->screen, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_opa(ui->screen, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui->screen, lv_color_hex(0x7e7e7e), LV_PART_MAIN | LV_STATE_DEFAULT);

        ui->screen_label_1 = lv_label_create(ui->screen);
        lv_label_set_text(ui->screen_label_1, "Smart Incubator");
        lv_obj_set_style_text_color(ui->screen_label_1, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui->screen_label_1, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_center(ui->screen_label_1);

        ui->screen_label_2 = lv_label_create(ui->screen);
        lv_label_set_text(ui->screen_label_2, "Initializing......");
        lv_obj_set_style_text_color(ui->screen_label_2, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui->screen_label_2, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(ui->screen_label_2, LV_ALIGN_BOTTOM_MID, 0, -20);

        ui->screen_del = false;
    }
    lv_obj_update_layout(ui->screen);
}
