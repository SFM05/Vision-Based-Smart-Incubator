#include "save_bmp.hpp"
#include "esp_log.h"
#include <cstdint>
#include <cstdio>

static const char *TAG = "save_bmp";

namespace yolo26
{

    namespace
    {

        constexpr int align_up(int num, int align) noexcept
        {
            return (num + align - 1) & ~(align - 1);
        }

        struct __attribute__((packed)) bmp_file_header_t
        {
            uint16_t file_type = 0x4D42;
            uint32_t file_size = 0;
            uint16_t reserved1 = 0;
            uint16_t reserved2 = 0;
            uint32_t offset_data = 66;
        };

        struct __attribute__((packed)) bmp_info_header_t
        {
            uint32_t size = 40;
            int32_t width = 0;
            int32_t height = 0;
            uint16_t planes = 1;
            uint16_t bit_count = 16;
            uint32_t compression = 3;
            uint32_t size_image = 0;
            int32_t x_pixels_per_meter = 0;
            int32_t y_pixels_per_meter = 0;
            uint32_t colors_used = 0;
            uint32_t colors_important = 0;
            uint32_t red_mask = 0xF800;
            uint32_t green_mask = 0x07E0;
            uint32_t blue_mask = 0x001F;
        };

    } // namespace

    esp_err_t write_bmp_rgb565le(const dl::image::img_t &img, const char *file_name)
    {
        if (img.pix_type != dl::image::DL_IMAGE_PIX_TYPE_RGB565LE)
        {
            ESP_LOGE(TAG, "Unsupported pixel type, only RGB565LE is allowed");
            return ESP_ERR_INVALID_ARG;
        }

        FILE *f = fopen(file_name, "wb");
        if (!f)
        {
            ESP_LOGE(TAG, "Failed to open %s", file_name);
            return ESP_FAIL;
        }

        auto row_stride = static_cast<size_t>(img.width) * sizeof(uint16_t);
        auto row_padded = static_cast<size_t>(align_up(static_cast<int>(row_stride), 4));
        auto padding_bytes = row_padded - row_stride;

        bmp_file_header_t file_hdr;
        bmp_info_header_t info_hdr;
        info_hdr.width = img.width;
        info_hdr.height = img.height;
        file_hdr.file_size = sizeof(file_hdr) + sizeof(info_hdr) +
                             static_cast<uint32_t>(row_padded * img.height);

        if (fwrite(&file_hdr, sizeof(file_hdr), 1, f) != 1 ||
            fwrite(&info_hdr, sizeof(info_hdr), 1, f) != 1)
        {
            ESP_LOGE(TAG, "Failed to write BMP headers");
            fclose(f);
            return ESP_FAIL;
        }

        // Write pixel data directly from source — no buffer, no conversion
        auto *src = static_cast<const uint8_t *>(img.data);
        for (int y = img.height - 1; y >= 0; --y)
        {
            if (fwrite(src + y * row_stride, row_stride, 1, f) != 1)
            {
                ESP_LOGE(TAG, "Failed to write pixel data");
                fclose(f);
                return ESP_FAIL;
            }
            if (padding_bytes > 0)
            {
                static const uint8_t zero[4] = {0};
                if (fwrite(zero, padding_bytes, 1, f) != 1)
                {
                    ESP_LOGE(TAG, "Failed to write row padding");
                    fclose(f);
                    return ESP_FAIL;
                }
            }
        }

        fclose(f);
        return ESP_OK;
    }
} // namespace yolo26
