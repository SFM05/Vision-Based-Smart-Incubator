#include "core/lv_obj_pos.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "frontend.hpp"
#include "gui_guider.h"
#include <atomic>
#include <cstdint>
#include <format>

// #include "tcd.h"
#include "widgets/lv_label.h"

#include "custom.hpp"

#ifdef __cplusplus
extern "C"
{
#endif
    // 温度滑动条回调
    // void slider_temp_event_cb(lv_event_t *e)
    // {
    //     if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED)
    //     {
    //         lv_obj_t *slider = lv_event_get_target(e);
    //         int32_t val = lv_slider_get_value(slider); // 范围0-100
    //         constexpr float MAX = 40.0;
    //         constexpr float MIN = 28.0;
    //         float temp = (MAX - MIN) * val / 100.0 + MIN;
    //         tcd_set_target(temp);
    //         // lv_lock();
    //         lv_label_set_text_fmt(guider_ui.screen_2_Temperature_2, "Temperature: %.1f", temp);
    //         lv_obj_invalidate(guider_ui.screen_2_Temperature_2);
    //         // lv_unlock();
    //     }
    // }

    // 环境上报间隔回调
    void dropdown_report_env_event_cb(lv_event_t *e)
    {
        auto delay = static_cast<std::atomic<uint16_t> *>(lv_event_get_user_data(e));
        if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED)
        {
            lv_obj_t *dd = lv_event_get_target(e);
            uint16_t idx = lv_dropdown_get_selected(dd);
            // idx: 0="1s", 1="15s", 2="30s"
            constexpr uint16_t DELAYS[] = {1000, 15 * 1000, 30 * 1000};
            delay->store(DELAYS[idx]);
        }
    }
    // 捕获间隔回调
    void dropdown_capture_event_cb(lv_event_t *e)
    {
        auto args = static_cast<dropdown_capture_event_args *>(lv_event_get_user_data(e));
        if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED)
        {
            lv_obj_t *dd = lv_event_get_target(e);
            uint16_t idx = lv_dropdown_get_selected(dd);
            // idx: 0="30s", 1="10min", 2="30min"
            constexpr uint32_t DELAYS[] = {30 * 1000, 10 * 60 * 1000, 30 * 60 * 1000};
            args->delay.store(DELAYS[idx]);
            if (!args->running.load())
                xTaskAbortDelay(args->capture_handle);
        }
    }
#ifdef __cplusplus
}
#endif