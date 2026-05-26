#ifndef GUI_GUIDER_H
#define GUI_GUIDER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

typedef struct
{
    lv_obj_t *screen;
    bool screen_del;
    lv_obj_t *screen_label_1;
    lv_obj_t *screen_label_2;
    lv_obj_t *screen_1;
    bool screen_1_del;
    lv_obj_t *screen_1_canvas_1;
    lv_obj_t *screen_1_label_1;
    lv_obj_t *screen_1_btn_1;
    lv_obj_t *screen_1_btn_1_label;
    lv_obj_t *screen_2;
    bool screen_2_del;
    lv_obj_t *screen_2_btn_1;
    lv_obj_t *screen_2_btn_1_label;
    lv_obj_t *screen_2_label_1;
    lv_obj_t *screen_2_label_2;
    lv_obj_t *screen_2_label_3;
    lv_obj_t *screen_2_slider_1;
} lv_ui;

void setup_scr_screen(lv_ui *ui);
void setup_scr_screen_1(lv_ui *ui);
void setup_scr_screen_2(lv_ui *ui);
void setup_ui(lv_ui *ui);

extern lv_ui guider_ui;

#ifdef __cplusplus
}
#endif
#endif
