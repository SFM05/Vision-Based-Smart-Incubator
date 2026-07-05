#include "events_init.h"
#include <stdio.h>

static void btn_param_cb(lv_event_t *e)
{
    lv_ui *ui = (lv_ui *)lv_event_get_user_data(e);
    setup_scr_screen_2(ui);
    lv_scr_load(ui->screen_2);
}

static void btn_cam_pre_cb(lv_event_t *e)
{
    lv_ui *ui = (lv_ui *)lv_event_get_user_data(e);
    setup_scr_screen_1(ui);
    lv_scr_load(ui->screen_1);
}

static const int interval_table[] = {10, 20, 30, 40, 50, 60};

static void slider_interval_cb(lv_event_t *e)
{
    lv_ui *ui = (lv_ui *)lv_event_get_user_data(e);
    int idx = lv_slider_get_value(ui->screen_2_slider_1);
    if (idx < 0) idx = 0;
    if (idx > 5) idx = 5;
    int sec = interval_table[idx];

    lv_slider_set_value(ui->screen_2_slider_1, idx, LV_ANIM_OFF);

    char buf[32];
    if (sec < 60)
        lv_snprintf(buf, sizeof(buf), "Interval_time: %ds", sec);
    else
        lv_snprintf(buf, sizeof(buf), "Interval_time: 1min");
    lv_label_set_text(ui->screen_2_label_3, buf);
}

void events_init_screen_1(lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_1_btn_1, btn_param_cb, LV_EVENT_CLICKED, ui);
}

void events_init_screen_2(lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_2_btn_1, btn_cam_pre_cb, LV_EVENT_CLICKED, ui);
    lv_obj_add_event_cb(ui->screen_2_slider_1, slider_interval_cb, LV_EVENT_VALUE_CHANGED, ui);
    lv_event_send(ui->screen_2_slider_1, LV_EVENT_VALUE_CHANGED, NULL);
}
