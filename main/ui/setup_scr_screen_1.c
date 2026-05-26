#include "esp_heap_caps.h"
#include "gui_guider.h"
#include "events_init.h"
#include <string.h>
#include "esp_cache.h"
#include <stdlib.h>

void setup_scr_screen_1(lv_ui *ui)
{
    if (ui->screen_1_del)
    {
        ui->screen_1 = lv_obj_create(NULL);
        lv_obj_set_size(ui->screen_1, 480, 320);
        lv_obj_set_scrollbar_mode(ui->screen_1, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_bg_opa(ui->screen_1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(ui->screen_1, lv_color_hex(0x7e7e7e), LV_PART_MAIN | LV_STATE_DEFAULT);

        ui->screen_1_canvas_1 = lv_canvas_create(ui->screen_1);
        static void *canvas_buf = NULL;
        if (!canvas_buf)
        {
            size_t sz = LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(320, 320);
            canvas_buf = heap_caps_aligned_alloc(128, sz, MALLOC_CAP_DEFAULT);
            if (canvas_buf) memset(canvas_buf, 0, sz);
        }
        lv_canvas_set_buffer(ui->screen_1_canvas_1, canvas_buf, 320, 320, LV_IMG_CF_TRUE_COLOR_ALPHA);
        lv_canvas_fill_bg(ui->screen_1_canvas_1, lv_color_hex(0xffffff), 255);
        if (canvas_buf) {
            size_t sz = LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(320, 320);
            esp_cache_msync(canvas_buf, sz, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
        }
        lv_obj_set_pos(ui->screen_1_canvas_1, 0, 0);
        lv_obj_set_size(ui->screen_1_canvas_1, 320, 320);

        ui->screen_1_label_1 = lv_label_create(ui->screen_1);
        lv_label_set_text(ui->screen_1_label_1, "Current\nNum: 0");
        lv_label_set_long_mode(ui->screen_1_label_1, LV_LABEL_LONG_WRAP);
        lv_obj_set_pos(ui->screen_1_label_1, 325, 0);
        lv_obj_set_size(ui->screen_1_label_1, 150, 72);
        lv_obj_set_style_text_color(ui->screen_1_label_1, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui->screen_1_label_1, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

        ui->screen_1_btn_1 = lv_btn_create(ui->screen_1);
        lv_obj_set_pos(ui->screen_1_btn_1, 320, 270);
        lv_obj_set_size(ui->screen_1_btn_1, 160, 50);
        lv_obj_set_style_bg_color(ui->screen_1_btn_1, lv_color_hex(0x6a6a6a), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(ui->screen_1_btn_1, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(ui->screen_1_btn_1, 0, 0);

        ui->screen_1_btn_1_label = lv_label_create(ui->screen_1_btn_1);
        lv_label_set_text(ui->screen_1_btn_1_label, "Parameter");
        lv_obj_set_style_text_color(ui->screen_1_btn_1_label, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui->screen_1_btn_1_label, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_center(ui->screen_1_btn_1_label);

        events_init_screen_1(ui);
        ui->screen_1_del = false;
    }
    lv_obj_update_layout(ui->screen_1);
}
