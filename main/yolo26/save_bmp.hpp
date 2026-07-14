#pragma once

#include "dl_image_define.hpp"
#include "esp_err.h"

namespace yolo26
{

    /**
     * @brief Write RGB565LE img_t to BMP file without dynamic memory allocation.
     *
     * Uses BI_BITFIELDS (16-bit RGB565) BMP format.
     * Writes raw pixel data directly — no conversion, no intermediate buffer.
     *
     * @param img  Source RGB565LE image.
     * @param file_name  Output BMP file path.
     * @return esp_err_t  ESP_OK on success, ESP_FAIL/ESP_ERR_INVALID_ARG on error.
     */
    esp_err_t write_bmp_rgb565le(const dl::image::img_t &img, const char *file_name);

} // namespace yolo26
