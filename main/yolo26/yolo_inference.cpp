#include <vector>
#include <format>
#include "dl_model_base.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "dl_image.hpp"
#include "yolo26.hpp"
#include "yolo_inference.hpp"

static const char *TAG = "yolo_inference";

dl::image::img_t decode_jpeg(const uint8_t *jpg_data, size_t jpg_len)
{
    dl::image::jpeg_img_t jpeg_img{};
    jpeg_img.data = (void *)jpg_data;
    jpeg_img.data_len = jpg_len;

    return dl::image::sw_decode_jpeg(jpeg_img, dl::image::DL_IMAGE_PIX_TYPE_RGB888);
}

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
        resized_img.data = heap_caps_malloc(resized_img.bytes(), MALLOC_CAP_DEFAULT);

        dl::image::ImageTransformer transformer;
        transformer.set_src_img(img);
        transformer.set_dst_img(resized_img);
        transformer.transform();

        return resized_img;
    }

    return img;
}

dl::image::img_t crop(const dl::image::img_t img,
                      int left,
                      int top,
                      int right,
                      int bottom)
{
    int width = img.width;
    int height = img.height;

    if (left != 0 || top != 0 || right != width || bottom != height)
    {
        dl::image::img_t cropped_img;
        cropped_img.width = right - left;
        cropped_img.height = bottom - top;
        cropped_img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888;
        cropped_img.data = heap_caps_malloc(cropped_img.bytes(), MALLOC_CAP_DEFAULT);

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

std::vector<dl::detect::result_t> yolo_inference(dl::Model *model,
                                                 const dl::image::img_t &img,
                                                 YOLO26 &processor)
{
    esp_task_wdt_add(NULL);
    auto inputs = model->get_inputs();
    dl::TensorBase *input_tensor = inputs.begin()->second;

    int model_h = input_tensor->shape[1];
    int model_w = input_tensor->shape[2];

    if (img.height != model_h)
    {
        const std::string warning_msg = std::format("The height is not aligned with the model, expected {} but received {}.", model_h, img.height);
        ESP_LOGW(TAG, "%s", warning_msg.data());
        ESP_LOGW(TAG, "Automatically resizing the image. If this is what you want, ignore this warning.");
    }
    if (img.width != model_w)
    {
        const std::string warning_msg = std::format("The width is not aligned with the model, expected {} but received {}.", model_w, img.width);
        ESP_LOGW(TAG, "%s", warning_msg.data());
        ESP_LOGW(TAG, "Automatically resizing the image. If this is what you want, ignore this warning.");
    }

    // 预处理
    processor.preprocess(img);
    ESP_LOGI(TAG, "Preprocess finished");
    esp_task_wdt_reset();

    // 推理
    model->run();
    esp_task_wdt_reset();
    ESP_LOGI(TAG, "Inference finished");

    // 后处理
    auto results = processor.postprocess(model->get_outputs());
    ESP_LOGI(TAG, "Postprocess finished");
    esp_task_wdt_reset();

    esp_task_wdt_delete(NULL);

    return results;
}