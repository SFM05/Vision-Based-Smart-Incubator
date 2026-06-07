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

    auto img = dl::image::sw_decode_jpeg(jpeg_img, dl::image::DL_IMAGE_PIX_TYPE_RGB888);

    return Image::ImageAuto(img);
}

Image::ImageAuto resize(Image::ImageAuto img,
                        uint16_t width,
                        uint16_t height,
                        bool keep_ratio,
                        uint32_t background)
{
    if (img.width() < width || img.height() < height) [[unlikely]]
    {
        std::string warning_msg = std::format("Target size is bigger than source size, expected <{}x{} but received {}x{}.", img.width(), img.height(), width, height);
        ESP_LOGW(TAG, "%s", warning_msg.c_str());
    }

    if (img.width() == width && img.height() == height) [[unlikely]]
        return img;

    Image::ImageAuto resized_img(width, height);
    auto resized_img_t = resized_img.to_img_t();
    const auto source_img_t = img.to_img_t_const();

    dl::image::ImageTransformer transformer;
    transformer.set_src_img(source_img_t);
    transformer.set_dst_img(resized_img_t);

    if (keep_ratio)
    {
        float min_ratio = std::min(static_cast<float>(height) / img.height(),
                                   static_cast<float>(width) / img.width());

        uint16_t scaled_h = img.height() * min_ratio;
        uint16_t scaled_w = img.width() * min_ratio;

        uint16_t top = (height - scaled_h) / 2;
        uint16_t bottom = height - scaled_h - top;
        uint16_t left = (width - scaled_w) / 2;
        uint16_t right = width - scaled_w - left;

        std::array<uint8_t, 3> bg_color = {static_cast<uint8_t>((background & 0x00FF0000) >> 16),
                                           static_cast<uint8_t>((background & 0x0000FF00) >> 8),
                                           static_cast<uint8_t>(background & 0x000000FF)};

        transformer.set_dst_img_border({top, bottom, left, right});
        transformer.set_bg_value(bg_color);
    }

    transformer.transform();

    return resized_img;
}

std::vector<dl::detect::result_t> yolo_inference(dl::Model *model,
                                                 Image::ImageAuto &img,
                                                 YOLO26 &processor)
{
    if (model == nullptr) [[unlikely]]
    {
        ESP_LOGW(TAG, "Model is not initialized.");
        assert(false);
        return {};
    }
    if (img.data_const() == nullptr) [[unlikely]]
    {
        ESP_LOGW(TAG, "Image is not initialized.");
        assert(false);
        return {};
    }

    esp_task_wdt_add(NULL);
    auto inputs = model->get_inputs();
    dl::TensorBase *input_tensor = inputs.begin()->second;

    int model_h = input_tensor->shape[1];
    int model_w = input_tensor->shape[2];

    if (img.height() != model_h) [[unlikely]]
    {
        const std::string warning_msg = std::format("The height is not aligned with the model, expected {} but received {}.", model_h, img.height());
        ESP_LOGW(TAG, "%s", warning_msg.c_str());
        ESP_LOGW(TAG, "Automatically resizing the image. If this is what you want, ignore this warning.");
    }
    if (img.width() != model_w) [[unlikely]]
    {
        const std::string warning_msg = std::format("The width is not aligned with the model, expected {} but received {}.", model_w, img.width());
        ESP_LOGW(TAG, "%s", warning_msg.c_str());
        ESP_LOGW(TAG, "Automatically resizing the image. If this is what you want, ignore this warning.");
    }

    /**
     * @brief 预处理
     */
    processor.preprocess(img.to_img_t_const());
    img.reset();
    ESP_LOGI(TAG, "Preprocess finished");
    esp_task_wdt_reset();

    /**
     * @brief 推理
     */
    model->run();
    esp_task_wdt_reset();
    ESP_LOGI(TAG, "Inference finished");

    /**
     * @brief 后处理
     */
    auto results = processor.postprocess(model->get_outputs());
    ESP_LOGI(TAG, "Postprocess finished");
    esp_task_wdt_reset();

    esp_task_wdt_delete(NULL);

    return results;
}