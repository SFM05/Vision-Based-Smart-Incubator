#pragma once

#include <vector>
#include "dl_model_base.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "yolo26.hpp"

// static const char* TAG="YOLO_INFERENCE";

/**
 * @brief JPEG解码
 * 
 * @param jpg_data 图片的起始位置
 * @param jpg_len 图片的大小（end-begin）
 * @return dl::image::img_t，解码后的图片
 */
dl::image::img_t decode_jpeg(const uint8_t *jpg_data, size_t jpg_len)
{
    dl::image::jpeg_img_t jpeg_img = {
        .data = (void *)jpg_data,
        .data_len = jpg_len};
    return dl::image::sw_decode_jpeg(jpeg_img, dl::image::DL_IMAGE_PIX_TYPE_RGB888);
}

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
                        int width)
{
    if (height <= 0 || width <= 0)
        return img;

    if (img.width != width || img.height != height)
    {
        dl::image::img_t resized_img;
        resized_img.width = width;
        resized_img.height = height;
        resized_img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;
        resized_img.data = heap_caps_malloc(dl::image::get_img_byte_size(resized_img), MALLOC_CAP_DEFAULT);

        dl::image::ImageTransformer transformer;
        transformer.set_src_img(img);
        transformer.set_dst_img(resized_img);
        transformer.transform();

        return resized_img;
    }

    return img;
}

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
                      int bottom)
{
    int width = img.width;
    int height = img.height;

    if (left!=0 || top!=0 || right!=width || bottom!= height){
        dl::image::img_t cropped_img;
        cropped_img.width=right-left;
        cropped_img.height=bottom-top;
        cropped_img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;
        cropped_img.data = heap_caps_malloc(dl::image::get_img_byte_size(cropped_img), MALLOC_CAP_DEFAULT);

        dl::image::ImageTransformer transformer;
        std::vector<int> crop_area = {left, top, right, bottom};
        transformer.set_src_img(img);
        transformer.set_dst_img(cropped_img);
        transformer.set_src_img_crop_area(crop_area);
        transformer.transform();
        return cropped_img;
    }
    
    return img;
}

/**
 * @brief 进行yolo26推理
 * 
 * @param model yolo26模型
 * @param img 解码后的图片
 * @param processor yolo26处理器
 */

std::vector<Detection> yolo_inference(dl::Model *model,
                                      dl::image::img_t img,
                                      Yolo26Processor &processor)
{
    auto inputs=model->get_inputs();
    dl::TensorBase* input_tensor = inputs.begin()->second;
        
    int model_h = input_tensor->shape[1];
    int model_w = input_tensor->shape[2];

    if (img.height != model_h)
        ESP_LOGE("YOLO_INFERENCE", "The height is not aligned with the model");
    if (img.width != model_w)
        ESP_LOGE("YOLO_INFERENCE", "The width is not aligned with the model");

    // 预处理
    TickType_t start_pre = xTaskGetTickCount();
    processor.preprocess(img, model->get_inputs());
    TickType_t end_pre = xTaskGetTickCount();
    // esp_task_wdt_reset();
    int lat_pre = (end_pre - start_pre) * portTICK_PERIOD_MS;
    ESP_LOGI("YOLO_INFERENCE", "Preprocess latency: %d ms", lat_pre);
    

    // 推理
    TickType_t start_inf = xTaskGetTickCount();
    model->run();
    TickType_t end_inf = xTaskGetTickCount();
    // esp_task_wdt_reset();
    int lat_inf = (end_inf - start_inf) * portTICK_PERIOD_MS;
    ESP_LOGI("YOLO_INFERENCE", "Inference latency: %d ms", lat_inf);

    // 后处理
    TickType_t start_post = xTaskGetTickCount();
    auto results = processor.postprocess(model->get_outputs());
    TickType_t end_post = xTaskGetTickCount();
    // esp_task_wdt_reset();
    int lat_post = (end_post - start_post) * portTICK_PERIOD_MS;
    ESP_LOGI("YOLO_INFERENCE", "Postprocess latency: %d ms", lat_post);

    return results;
}
