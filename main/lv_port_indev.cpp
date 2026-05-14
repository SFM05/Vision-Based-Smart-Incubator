#include "lv_port_indev.hpp"
#include "ft6336.hpp"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "indev";

/* Touch ADC range (from 4-corner calibration) */
#define TOUCH_X_MAX  305
#define TOUCH_Y_MAX  465

static bool was_pressed = false;

static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    touch_point_t pts[2];
    int count = ft6336_read_points(pts);

    if (count >= 1 && pts[0].active) {
        data->point.x = (TOUCH_Y_MAX - pts[0].y) * 479 / TOUCH_Y_MAX;
        data->point.y = pts[0].x * 319 / TOUCH_X_MAX;
        data->state = LV_INDEV_STATE_PRESSED;
        was_pressed = true;
    } else if (was_pressed) {
        data->state = LV_INDEV_STATE_RELEASED;
        was_pressed = false;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
}