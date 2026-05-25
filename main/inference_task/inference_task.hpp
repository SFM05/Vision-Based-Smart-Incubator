#pragma once

#include "lvgl.h"
#include "gui_guider.h"

#ifdef __cplusplus
extern "C" {
#endif

void start_inference_task(lv_ui *ui);
void inference_set_interval_seconds(int sec);

#ifdef __cplusplus
}
#endif
