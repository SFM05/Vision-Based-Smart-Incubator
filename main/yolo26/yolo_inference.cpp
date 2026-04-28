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
#include "image_auto.hpp"

static const char *TAG = "yolo_inference";

Image::ImageAuto decode_jpeg(const uint8_t *jpg_data, size_t jpg_len)
{
    dl::image::jpeg_img_t jpeg_img{};
    jpeg_img.data = (void *)jpg_data;
    jpeg_img.data_len = jpg_len;

    auto img=dl::image::sw_decode_jpeg(jpeg_img, dl::image::DL_IMAGE_PIX_TYPE_RGB888);

    return Image::ImageAuto(img);
}

Image::ImageAuto resize(Image::ImageAuto img,
                        int height,
                        int width)
{
    if (height <= 0 || width <= 0)
        return img;

    if (img.width() != width || img.height() != height)
    {
        Image::ImageAuto resized_img(width, height);
        auto resized_img_t = resized_img.to_img_t();
        const auto source_img_t = img.to_img_t_const();

        dl::image::ImageTransformer transformer;
        transformer.set_src_img(source_img_t);
        transformer.set_dst_img(resized_img_t);
        transformer.transform();

        return resized_img;
    }

    return img;
}

Image::ImageAuto crop(Image::ImageAuto img,
                      int left,
                      int top,
                      int right,
                      int bottom)
{
    int width = img.width();
    int height = img.height();

    if (left != 0 || top != 0 || right != width || bottom != height)
    {
        Image::ImageAuto cropped_img(width, height);
        auto cropped_img_t = cropped_img.to_img_t();
        const auto source_img_t = img.to_img_t_const();

        dl::image::ImageTransformer transformer;
        std::vector<int> crop_area = {left, top, right, bottom};
        transformer.set_src_img(source_img_t);
        transformer.set_dst_img(cropped_img_t);
        transformer.set_src_img_crop_area(crop_area);
        transformer.transform();

        return cropped_img;
    }

    return img;
}

std::vector<dl::detect::result_t> yolo_inference(dl::Model *model,
                                                 Image::ImageAuto &img,
                                                 YOLO26 &processor)
{
    esp_task_wdt_add(NULL);
    auto inputs = model->get_inputs();
    dl::TensorBase *input_tensor = inputs.begin()->second;

    int model_h = input_tensor->shape[1];
    int model_w = input_tensor->shape[2];

    if (img.height() != model_h)
    {
        const std::string warning_msg = std::format("The height is not aligned with the model, expected {} but received {}.", model_h, img.height());
        ESP_LOGW(TAG, "%s", warning_msg.data());
        ESP_LOGW(TAG, "Automatically resizing the image. If this is what you want, ignore this warning.");
    }
    if (img.width() != model_w)
    {
        const std::string warning_msg = std::format("The width is not aligned with the model, expected {} but received {}.", model_w, img.width());
        ESP_LOGW(TAG, "%s", warning_msg.data());
        ESP_LOGW(TAG, "Automatically resizing the image. If this is what you want, ignore this warning.");
    }

    // 预处理
    processor.preprocess(img.to_img_t_const());
    img.reset();
    ESP_LOGI(TAG, "Preprocess finished");
    
    heap_caps_check_integrity_all(true);
    esp_task_wdt_reset();
    

    // 推理
    model->run();
    esp_task_wdt_reset();
    ESP_LOGI(TAG, "Inference finished");
    heap_caps_check_integrity_all(true);

    // 后处理
    auto results = processor.postprocess(model->get_outputs());
    ESP_LOGI(TAG, "Postprocess finished");
    heap_caps_check_integrity_all(true);
    esp_task_wdt_reset();

    esp_task_wdt_delete(NULL);

    return results;
}