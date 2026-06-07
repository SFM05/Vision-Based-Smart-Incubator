#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"
#include "gui_guider.h"

#ifdef __cplusplus
extern "C" {
#endif

void start_inference_task(lv_ui *ui);
void inference_set_interval_seconds(int sec);
bool inference_is_running(void);

esp_err_t inference_get_raw_snapshot(uint8_t **out_buf, size_t *out_size);

#ifdef __cplusplus
}
#endif
