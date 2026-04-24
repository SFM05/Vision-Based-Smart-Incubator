#pragma once

#include <vector>
#include "dl_model_base.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "dl_image.hpp"
#include "yolo26.hpp"

/**
 * @brief JPEG解码
 * 
 * @param jpg_data 图片的起始位置
 * @param jpg_len 图片的大小（end-begin）
 * @return dl::image::img_t，解码后的图片
 */
dl::image::img_t decode_jpeg(const uint8_t *jpg_data, size_t jpg_len);


/**
 * @brief 对图片进行缩放
 * 
 * @param img 解码后的图片
 * @param height 目标图片的高
 * @param width 目标图片的宽
 * @return dl::image::img_t，缩放后的图片
 */
dl::image::img_t resize(const dl::image::img_t &img,
                        int height,
                        int width);


/**
 * @brief 对图片进行裁剪
 * 
 * @param img 解码后的图片
 * @param left 左边界
 * @param top 上边界
 * @param right 右边界
 * @param bottom 下边界
 * @return dl::image::img_t，裁剪后的图片
 */
dl::image::img_t crop(const dl::image::img_t img,
                      int left,
                      int top,
                      int right,
                      int bottom);


/**
 * @brief 进行yolo26推理
 * 
 * @param model yolo26模型
 * @param img 解码后的图片
 * @param processor yolo26处理器
 */
std::vector<dl::detect::result_t> yolo_inference(dl::Model *model,
                                                 const dl::image::img_t &img,
                                                 YOLO26 &processor);
