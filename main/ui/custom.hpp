#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "gui_guider.h"
#include <atomic>
#include <cstdint>

#ifdef __cplusplus
extern "C"
{
#endif
    void slider_temp_event_cb(lv_event_t *e);         // 温度滑动条回调
    void dropdown_report_env_event_cb(lv_event_t *e); // 环境上报间隔回调
    struct dropdown_capture_event_args
    {
        std::atomic<uint32_t> &delay;
        std::atomic<bool> &running;
        TaskHandle_t capture_handle;
    };
    void dropdown_capture_event_cb(lv_event_t *e); // 捕获间隔回调
#ifdef __cplusplus
}
#endif
