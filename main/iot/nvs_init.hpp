#pragma once

#include "esp_err.h"

namespace NVS_init
{
    inline bool is_initialized = 0;
    esp_err_t nvs_init();
}