#pragma once

#include <vector>
#include "dl_model_base.hpp"
#include "dl_image.hpp"
#include "yolo26.hpp"
#include "image_auto.hpp"

/**
 * @brief JPEG解码
 *
 * @param jpg_data 图片的起始位置
 * @param jpg_len 图片的大小（end-begin）
 * @return Image::ImageAuto 解码后的图片
 * 
 * @see Image::ImageAuto
 */
Image::ImageAuto decode_jpeg(const uint8_t *jpg_data, size_t jpg_len);

/**
 * @brief 对图片直接进行缩放
 *
 * @param img 解码后的图片
 * @param height 目标图片的高
 * @param width 目标图片的宽
 * @param keep_ratio 是否保持比例一致
 * @param background 填充的颜色，默认为灰色 (114, 114, 114)，该参数只会在keep_ratio为true时生效
 * @return Image::ImageAuto 缩放后的图片
 * 
 * @see Image::ImageAuto
 */
Image::ImageAuto resize(Image::ImageAuto img,
                        uint16_t width,
                        uint16_t height,
                        bool keep_ratio,
                        uint32_t background = 0x00727272);

/**
 * @brief 进行yolo26推理
 *
 * @param model yolo26模型
 * @param img 解码后的图片
 * @param processor yolo26处理器
 * @return std::vector<dl::detect::result_t> 推理结果
 * 
 * @see dl::detect::result_t
 */
std::vector<dl::detect::result_t> yolo_inference(dl::Model *model,
                                                 Image::ImageAuto &img,
                                                 YOLO26 &processor);
