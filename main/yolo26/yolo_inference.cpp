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
        std::string warning_msg = std::format("Target size is bigger than the size, expected <{}x{} but received {}x{}.", img.width(), img.height(), width, height);
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

Image::ImageAuto resize(Image::ImageAuto img,
                        uint16_t width,
                        uint16_t height,
                        std::array<int, 4> border,
                        uint32_t background)
{
    if (img.width() < width || img.height() < height) [[unlikely]]
    {
        std::string warning_msg = std::format("Target size is bigger than the size, expected <{}x{} but received {}x{}.", img.width(), img.height(), width, height);
        ESP_LOGW(TAG, "%s", warning_msg.c_str());
    }
    if (border[0] + border[1] >= width) [[unlikely]]
    {
        std::string warning_msg = std::format("Horizontal border is longer than the width, expected <{} but received {}+{}.", width, border[0], border[1]);
        ESP_LOGW(TAG, "%s", warning_msg.c_str());
        assert(false);
        return img;
    }
    if (border[2] + border[3] >= height) [[unlikely]]
    {
        std::string warning_msg = std::format("Vertical border is longer than the height, expected <{} but received {}+{}.", height, border[2], border[3]);
        ESP_LOGW(TAG, "%s", warning_msg.c_str());
        assert(false);
        return img;
    }

    if (img.width() == width && img.height() == height) [[unlikely]]
        return img;

    Image::ImageAuto resized_img(width, height);
    auto resized_img_t = resized_img.to_img_t();
    auto source_img_t = img.to_img_t_const();

    dl::image::ImageTransformer transformer;
    std::array<uint8_t, 3> bg_color = {static_cast<uint8_t>((background & 0x00FF0000) >> 16),
                                       static_cast<uint8_t>((background & 0x0000FF00) >> 8),
                                       static_cast<uint8_t>(background & 0x000000FF)};

    transformer.set_src_img(source_img_t);
    transformer.set_dst_img(resized_img_t);
    transformer.set_dst_img_border(std::vector<int>(border.begin(), border.end()));
    transformer.set_bg_value(bg_color);
    transformer.transform();

    return resized_img;
}

Image::ImageAuto crop(Image::ImageAuto img,
                      std::array<int, 4> crop_area)
{
    int left = crop_area[0];
    int top = crop_area[1];
    int right = crop_area[2];
    int bottom = crop_area[3];

    if (left >= right) [[unlikely]]
    {
        std::string error_msg = std::format("Crop area is invalid, expected left<right but received left={} right={}.", left, right);
        ESP_LOGE(TAG, "%s", error_msg.c_str());
        assert(false);
        return img;
    }
    if (top >= bottom) [[unlikely]]
    {
        std::string error_msg = std::format("Crop area is invalid, expected top<bottom but received top={} bottom={}.", top, bottom);
        ESP_LOGE(TAG, "%s", error_msg.c_str());
        assert(false);
        return img;
    }
    if (left < 0) [[unlikely]]
    {
        std::string warning_msg = std::format("Crop area is invalid, expected left>=0 but received left={}.", left);
        ESP_LOGW(TAG, "%s", warning_msg.c_str());
        left = 0;
        ESP_LOGW(TAG, "left is set to 0.");
    }
    if (right < 0) [[unlikely]]
    {
        std::string warning_msg = std::format("Crop area is invalid, received left={}<0.", right);
        ESP_LOGW(TAG, "%s", warning_msg.c_str());
        right = 0;
        ESP_LOGW(TAG, "left is set to 0.");
    }
    if (right > img.width()) [[unlikely]]
    {
        std::string warning_msg = std::format("Crop area is invalid, expected right<=image width but received right={} width={}.", right, img.width());
        ESP_LOGW(TAG, "%s", warning_msg.c_str());
        right = img.width();
        ESP_LOGW(TAG, "right is set to image width.");
    }
    if (bottom > img.height()) [[unlikely]]
    {
        std::string warning_msg = std::format("Crop area is invalid, expected bottom<=image height but received bottom={} height={}.", bottom, img.height());
        ESP_LOGW(TAG, "%s", warning_msg.c_str());
        bottom = img.height();
        ESP_LOGW(TAG, "bottom is set to image height.");
    }

    if (left == 0 && top == 0 && right == img.width() && bottom == img.height()) [[unlikely]]
        return img;

    Image::ImageAuto cropped_img(right - left, bottom - top);
    auto cropped_img_t = cropped_img.to_img_t();
    const auto source_img_t = img.to_img_t_const();

    dl::image::ImageTransformer transformer;
    transformer.set_src_img(source_img_t);
    transformer.set_dst_img(cropped_img_t);
    transformer.set_src_img_crop_area(std::vector<int>(crop_area.begin(), crop_area.end()));
    transformer.transform();

    return cropped_img;
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

    // 预处理
    processor.preprocess(img.to_img_t_const());
    img.reset();
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