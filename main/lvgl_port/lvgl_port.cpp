#include "lvgl_port.hpp"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_cache.h"

#include "inference_task/inference_task.hpp"

MirrorConfig LvglPort::s_mirror{};

static const char *TAG = "lvgl_port";

static void lvgl_tick_task(void *arg)
{
    lv_tick_inc(1);
}

/**
 * @brief 全区域 flush — 一次绘制整个脏矩形区域
 */
static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)drv->user_data;
    if (!panel) { lv_disp_flush_ready(drv); return; }

    size_t pixel_cnt = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
    size_t flush_sz = pixel_cnt * sizeof(uint16_t);
    esp_cache_msync((void *)color_map, flush_sz,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);

    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1,
                              area->x2 + 1, area->y2 + 1,
                              color_map);
    lv_disp_flush_ready(drv);
}

static void lvgl_touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t touch = (esp_lcd_touch_handle_t)drv->user_data;
    if (!touch) { data->state = LV_INDEV_STATE_REL; return; }

    if (inference_is_running()) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    esp_err_t err = esp_lcd_touch_read_data(touch);
    if (err != ESP_OK) {
        err = esp_lcd_touch_read_data(touch);
    }
    if (err != ESP_OK) {
        data->state = LV_INDEV_STATE_REL; return;
    }

    esp_lcd_touch_point_data_t point;
    uint8_t cnt = 0;
    esp_lcd_touch_get_data(touch, &point, &cnt, 1);

    if (cnt > 0) {
        int rx = point.x;
        int ry = point.y;

        const auto& m = LvglPort::mirror();
        if (m.mv) {
            int src_x = m.my ? (477 - ry) : ry;
            int src_y = m.mx ? rx : (319 - rx);
            data->point.x = src_x * 479 / 477;
            data->point.y = src_y;
        } else {
            int src_x = m.mx ? (319 - rx) : rx;
            int src_y = m.my ? (477 - ry) : ry;
            data->point.x = src_x * 479 / 319;
            data->point.y = src_y * 319 / 477;
        }
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

esp_err_t LvglPort::init(
    esp_lcd_panel_handle_t panel,
    esp_lcd_panel_io_handle_t io,
    esp_lcd_touch_handle_t touch,
    uint16_t width,
    uint16_t height)
{
    size_t buf_pixels = width * (height / 4);
    size_t buf_bytes = buf_pixels * sizeof(lv_color_t);

    void *buf1 = heap_caps_aligned_alloc(128, buf_bytes,
                MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    void *buf2 = heap_caps_aligned_alloc(128, buf_bytes,
                MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "Failed to allocate LVGL draw buffers");
        return ESP_ERR_NO_MEM;
    }
    memset(buf1, 0, buf_bytes);
    memset(buf2, 0, buf_bytes);
    esp_cache_msync(buf1, buf_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    esp_cache_msync(buf2, buf_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);

    lv_init();

    esp_timer_create_args_t tick_timer_args = {};
    tick_timer_args.callback = &lvgl_tick_task;
    tick_timer_args.name = "lvgl_tick";
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 1000));

    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_pixels);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = width;
    disp_drv.ver_res = height;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = (void *)panel;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read_cb;
    indev_drv.user_data = (void *)touch;
    lv_indev_drv_register(&indev_drv);

    ESP_LOGI(TAG, "LVGL initialized: %dx%d, buf1=%p buf2=%p", width, height, buf1, buf2);
    return ESP_OK;
}

void LvglPort::tick()
{
    lv_timer_handler();
}

void LvglPort::refresh()
{
    lv_disp_t *d = lv_disp_get_default();
    if (d) lv_refr_now(d);
}

void LvglPort::set_mirror(const MirrorConfig& cfg)
{
    s_mirror = cfg;
}
