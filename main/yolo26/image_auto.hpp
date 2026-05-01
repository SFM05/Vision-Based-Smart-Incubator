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

        void detach();

    public:
        /**
         * @brief 创建一个ImageAuto对象
         * 
         * @param width 图片宽度
         * @param height 图片高度
         * @param pix_type 像素类型，默认为RGB888
         */
        ImageAuto(uint16_t width,
                  uint16_t height,
                  dl::image::pix_type_t pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888);
        /**
         * @brief 从img_t创建一个ImageAuto对象
         * 
         * @param img 创建的来源
         */
        ImageAuto(const dl::image::img_t &img);
        ImageAuto(const ImageAuto &) = default;
        ImageAuto(ImageAuto &&) = default;

        ~ImageAuto() = default;

        ImageAuto &operator=(const ImageAuto &) = default;
        ImageAuto &operator=(ImageAuto &&) = default;

        uint16_t width() const;
        uint16_t height() const;
        dl::image::pix_type_t pix_type() const;
        /**
         * @brief 获取指向图片的常量指针
         * 
         * @return std::shared_ptr<const uint8_t[]>
         */
        std::shared_ptr<const uint8_t[]> data_const() const;
        /**
         * @brief 获取指向图片的指针
         * 
         * @return std::shared_ptr<uint8_t[]>
         */
        std::shared_ptr<uint8_t[]> data();
        size_t size() const;

        /**
         * @brief 获取常量兼容类型dl::image::img_t
         * 
         * @return const dl::image::img_t
         * 
         * @note 该方法仅推荐在面向老代码时使用。
         * 请保证返回的指针不会被释放，且指向的内容不被修改。
         */
        const dl::image::img_t to_img_t_const();
        /**
         * @brief 获取兼容类型dl::image::img_t
         * 
         * @return dl::image::img_t
         * 
         * @note 该方法仅推荐在面向老代码时使用。
         * 请保证返回的指针不会被释放。
         */
        dl::image::img_t to_img_t();

        /**
         * @brief 释放图片
         */
        void reset();
    };
}