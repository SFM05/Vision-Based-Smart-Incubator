#include "lv_port_disp.hpp"
#include "st7796.hpp"
#include "lvgl.h"
#include "esp_log.h"

#if LV_USE_LOG != 0
static const char *TAG = "lv_port_disp";
#endif

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    st7796_flush(area->x1, area->y1, area->x2, area->y2, (const uint16_t *)color_p);
    lv_disp_flush_ready(disp_drv);
}

void lv_port_disp_init(void)
{
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf[160 * 40];
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, 160 * 40);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 160;
    disp_drv.ver_res = 320;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
}