#pragma once

#include "esp_err.h"

namespace IOT
{
    esp_err_t ntp_time_sync();

    esp_err_t upload_file(const char *url,           // 预签名 URL（含 query 参数）
                          const void *data,       // JPEG 数据指针
                          size_t len);                // 数据长度
}
