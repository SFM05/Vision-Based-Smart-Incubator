#pragma once

#include <cstdint>
#include "esp_err.h"

#define CAM_DISP_W   320
#define CAM_DISP_H   240

extern uint16_t cam_fb[CAM_DISP_W * CAM_DISP_H];
extern volatile bool cam_frame_ready;

esp_err_t camera_init(void);
