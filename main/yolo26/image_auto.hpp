#pragma once
#include <memory>
#include "dl_image.hpp"

namespace Image
{
    class ImageAuto
    {
    private:
        uint16_t m_width;
        uint16_t m_height;
        dl::image::pix_type_t m_pix_type;
        std::shared_ptr<uint8_t[]> m_data;

        void datach();

    public:
        ImageAuto(uint16_t width,
                  uint16_t height,
                  dl::image::pix_type_t pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888);
        ImageAuto(const dl::image::img_t &img);
        ImageAuto(const ImageAuto &) = default;
        ImageAuto(ImageAuto &&) = default;

        ~ImageAuto() = default;

        ImageAuto &operator=(const ImageAuto &) = default;
        ImageAuto &operator=(ImageAuto &&) = default;

        uint16_t width();
        uint16_t height();
        dl::image::pix_type_t pix_type();
        // std::shared_ptr<void> &data();
        // void *raw_data();

        const dl::image::img_t to_img_t_const();
        dl::image::img_t to_img_t();

        void reset();
    };
}