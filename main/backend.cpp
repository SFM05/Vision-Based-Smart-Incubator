#include "esp_heap_caps.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
namespace fs = std::filesystem;
#include "cJSON.h"
#include "dl_image_define.hpp"
#include "dl_image_jpeg.hpp"
#include "dl_image_process.hpp"
#include "dl_model_base.hpp"
#include "esp_cache.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "linux/videodev2.h"
#include "portmacro.h"
#include "yolo26.hpp"
#include <sys/mman.h>
#include <vector>
// #include "esp_video_device.h"
// #include "esp_video_isp_ioctl.h"

#include "abnormal_detect.hpp"
#include "cam.hpp"
#include "frontend.hpp"
#include "iot_utils.hpp"
#include "mqtt.hpp"
#include "sdcard.hpp"
#include "sensor.h"

#include "backend.hpp"

static const char *TAG = "backend";
static constexpr int CROP_SIZE = 640;

void capture_process(void *argv)
{
    const char *TAG = "capture";
    ESP_LOGI(TAG, "Capture process started.");
    auto args = static_cast<capture_process_args *>(argv);
    TickType_t last_wake_time;
    vTaskDelay(pdMS_TO_TICKS(1000));

    v4l2_buffer cam_buf{};
    cam_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam_buf.memory = V4L2_MEMORY_MMAP;

    uint8_t *frame = nullptr;

    while (true)
    {
        args->running = true;
        if (!CAM::camera::instance().streaming())
        {
            vTaskDelay(pdMS_TO_TICKS(1));
            args->running = false;
            continue;
        }

        // 获取唤醒时间
        last_wake_time = xTaskGetTickCount();
        vTaskDelay(pdMS_TO_TICKS(500));

        // 接管帧缓冲区
        auto state = ioctl(CAM::camera::instance().fd(), VIDIOC_DQBUF, &cam_buf);
        if (state < 0) // 缓冲帧未准备完毕
        {
            ioctl(CAM::camera::instance().fd(), VIDIOC_QBUF, &cam_buf);
            continue;
        }

        if (cam_buf.index >= static_cast<uint32_t>(CAM::camera::instance().num_bufs()))
        {
            ioctl(CAM::camera::instance().fd(), VIDIOC_QBUF, &cam_buf);
            continue;
        }

        frame = static_cast<uint8_t *>(CAM::camera::instance().buf_start()[cam_buf.index]); // 获取当前帧
        // s_cap.last_index = cam_buf.index;
        esp_cache_msync(frame, CAM::camera::instance().buf_len()[cam_buf.index],
                        ESP_CACHE_MSYNC_FLAG_DIR_M2C); // 同步缓冲

        dl::image::img_t raw{
            nullptr,
            CAM::camera::instance().width(),
            CAM::camera::instance().height(),
            dl::image::DL_IMAGE_PIX_TYPE_RGB565LE};
        dl::image::ImageTransformer transformer;
        raw.data = static_cast<void *>(frame);
        // ESP_LOGI(TAG,"%d %d->%d %d",raw.height,raw.width,args->cropped_buf.height,args->cropped_buf.width);
        transformer.set_src_img(raw);
        transformer.set_dst_img(args->cropped_buf);
        transformer.set_src_img_crop_area({420, 0, 1499, 1079});
        transformer.transform();
        ioctl(CAM::camera::instance().fd(), VIDIOC_QBUF, &cam_buf); // 归还帧缓冲区
        ESP_LOGI(TAG, "Get one frame.");

        // 帧信息
        uint8_t plate_id = 0;
        time_t now = time(NULL); // 秒级时间戳
        tm tm_info;
        localtime_r(&now, &tm_info); // 转本地时间
        frame_meta_data data{"\0", plate_id};
        strftime(data.time, 20, "%Y%m%d-%H%M%S", &tm_info);

        vTaskDelay(pdMS_TO_TICKS(1));
        xQueueSend(args->start_inference, &data, pdMS_TO_TICKS(200));

        taskYIELD(); // 进入就绪态

        // 编码输出至sd卡
        if (sdcard::SDcard::instance().status())
        {
            auto jpeg = dl::image::hw_encode_jpeg(args->cropped_buf);

            fs::path file_path = std::format("/sdcard/{}/{}.jpg",
                                             data.plate_id,
                                             data.time);
            if (!fs::exists(file_path.parent_path())) [[unlikely]] // 创建父目录
                fs::create_directories(file_path.parent_path());
            std::ofstream file(file_path, std::ios::binary);
            if (!file.is_open()) [[unlikely]]
            {
                ESP_LOGE(TAG, "Cannot openfile: %s", file_path.c_str());
                continue;
            }

            file.write(static_cast<char *>(jpeg.data), jpeg.data_len);
            file.close(); // 关闭文件
            heap_caps_free(jpeg.data);
            ESP_LOGI(TAG, "Write to path: %s", file_path.c_str());
        }

        xSemaphoreTake(args->inference_end, portMAX_DELAY);
        // 通知上传任务
        xQueueSend(args->request_upload, &data, portMAX_DELAY);
        xSemaphoreGive(args->flush_screen);
        ESP_LOGI(TAG, "%d", uxTaskGetStackHighWaterMark(nullptr));
        args->running = false;
        xTaskDelayUntil(&last_wake_time,
                        pdMS_TO_TICKS(args->delay.load()));
    }
}

void inference_process(void *argv)
{
    const char *TAG = "inference";
    ESP_LOGI(TAG, "Inference process start.");
    auto args = static_cast<inference_process_args *>(argv);

    frame_meta_data data;

    while (true)
    {
        // xSemaphoreTake(args->start_inference, portMAX_DELAY); // 等待启动
        xQueueReceive(args->start_inference, &data, portMAX_DELAY);
        ESP_LOGI(TAG, "Start inference...");

        xSemaphoreTake(args->capture_inference_mutex, portMAX_DELAY); // 加锁保证多线程
        args->processor.preprocess(args->cropped_buf);
        xSemaphoreGive(args->capture_inference_mutex);
        ESP_LOGI(TAG, "Preprocess finished.");
        taskYIELD();

        args->model->run();
        ESP_LOGI(TAG, "Inference finished.");
        taskYIELD();

        args->result = args->processor.postprocess(args->model->get_outputs());
        ESP_LOGI(TAG, "Postprocess finished.");
        ESP_LOGI(TAG, "Detected %d clonies.", static_cast<int>(args->result.size()));
        taskYIELD();

        // 输出至sd卡
        if (sdcard::SDcard::instance().status())
        {
            fs::path file_path = std::format("/sdcard/{}/{}.txt",
                                             data.plate_id,
                                             data.time);
            if (!fs::exists(file_path.parent_path())) [[unlikely]] // 创建父目录
                fs::create_directories(file_path.parent_path());
            std::ofstream file(file_path, std::ios::out);
            if (!file.is_open()) [[unlikely]]
            {
                ESP_LOGE(TAG, "Cannot openfile: %s", file_path.c_str());
                continue;
            }

            for (const auto &item : args->result)
            {
                file << std::format("{:<3} {:<3} {:<3} {:<3} {:<8.6f}\n",
                                    item.box[0],
                                    item.box[1],
                                    item.box[2],
                                    item.box[3],
                                    item.score);
            }
            file.close();
            ESP_LOGI(TAG, "Write to path: %s", file_path.c_str());
        }

        xSemaphoreGive(args->start_detect);

        // 通知上传任务
        // xSemaphoreGive(args->inference_end);
        ESP_LOGI(TAG, "%d", uxTaskGetStackHighWaterMark(nullptr));
    }
}

void detect_abnormal_process(void *argv)
{
    const char *TAG = "detect abnormal";
    auto args = static_cast<detect_abnormal_process_args *>(argv);

    float *base_feature;
    bool status;
    constexpr int TARGET_K = 128;
    base_feature = static_cast<float *>(heap_caps_malloc(23 * TARGET_K * sizeof(float),
                                                         MALLOC_CAP_SPIRAM));
    float mean_feature[23];
    dl::image::img_t resized_img{
        nullptr,
        640,
        640,
        dl::image::DL_IMAGE_PIX_TYPE_RGB888};
    resized_img.data = heap_caps_malloc(resized_img.bytes(), MALLOC_CAP_SPIRAM);
    dl::image::ImageTransformer transformer;

    while (true)
    {
        xSemaphoreTake(args->start_detect, portMAX_DELAY);

        transformer.set_src_img(args->cropped_buf);
        transformer.set_dst_img(args->cropped_buf);
        transformer.transform();
        taskYIELD();

        for (size_t i = 0; i < args->result.size(); i++)
            extract_base_features(resized_img,
                                  args->result[i],
                                  base_feature + 23 * i);
        extract_mean_feature(base_feature, mean_feature, args->result.size());
        taskYIELD();
        status = false;
        for (size_t i = 0; i < args->result.size(); i++)
        {
            bool res = inference(resized_img, args->result, i, base_feature, mean_feature);
            if (res)
            {
                status = true;
                break;
            }
        }
        xSemaphoreGive(args->inference_end);
        xQueueSend(args->status, &status, pdMS_TO_TICKS(500));
        ESP_LOGI(TAG, "%d", uxTaskGetStackHighWaterMark(nullptr));
    }
}

void upload_request_process(void *argv)
{
    const char *TAG = "upload reqest";
    auto args = static_cast<upload_request_process_args *>(argv);
    frame_meta_data data;

    while (true)
    {
        xQueueReceive(args->request_upload, &data, portMAX_DELAY);

        std::string img_path = std::format("/sdcard/{}/{}.jpg",
                                           data.plate_id,
                                           data.time);
        std::string txt_path = std::format("/sdcard/{}/{}.txt",
                                           data.plate_id,
                                           data.time);
        cJSON *root = cJSON_CreateObject();
        // NTP同步失败，时间位1970。上报空时间戳，使用服务器时间
        if (data.time[0] != '2')
            cJSON_AddStringToObject(root, "timestamp", "");
        else
            cJSON_AddStringToObject(root, "timestamp", data.time);

        cJSON_AddNumberToObject(root, "plateid", data.plate_id);
        cJSON_AddStringToObject(root, "imgpath", img_path.c_str());
        cJSON_AddStringToObject(root, "txtpath", txt_path.c_str());
        cJSON_AddNumberToObject(root, "number", args->result.size());
        char *upload_msg = cJSON_PrintUnformatted(root);

        int err = esp_mqtt_client_publish(MQTT::MQTT::instance().mqtt_handle(),
                                          std::format("device/{}/upload", IOT::get_mac()).c_str(),
                                          upload_msg,
                                          0,
                                          1,
                                          0);
        if (err == 0)
            ESP_LOGI(TAG, "Request: %s.", upload_msg);
        else
            ESP_LOGI(TAG, "Fail to send request, return code: %d", err);
        cJSON_free(upload_msg);
        cJSON_Delete(root);
        ESP_LOGI(TAG, "%d", uxTaskGetStackHighWaterMark(nullptr));
    }
}

void upload_process(void *argv)
{
    const char *TAG = "upload";
    auto args = static_cast<upload_process_args *>(argv);
    char msg[512];

    while (true)
    {
        xQueueReceive(args->msg_queue, msg, portMAX_DELAY);
        cJSON *root = cJSON_Parse(msg);
        if (root == nullptr)
        {
            ESP_LOGE(TAG, "Parse JSON error.");
            continue;
        }

        if (cJSON_GetObjectItem(root, "success")->type == cJSON_True)
        {
            IOT::upload_file(cJSON_GetObjectItem(root, "url")->valuestring,
                             cJSON_GetObjectItem(root, "path")->valuestring,
                             args->buffer,
                             args->size);
            ESP_LOGI(TAG, "Upload %s to %s.",
                     cJSON_GetObjectItem(root, "url")->valuestring,
                     cJSON_GetObjectItem(root, "path")->valuestring);
        }
        else
        {
            ESP_LOGW(TAG, "Server sign URL failed.");
        }
        cJSON_Delete(root);
        ESP_LOGI(TAG, "%d", uxTaskGetStackHighWaterMark(nullptr));
    }
}

void report_env_process(void *argv)
{
    const char *TAG = "report env";
    auto args = static_cast<report_env_process_args *>(argv);
    sensors_data_t data;
    char current_day[20];
    char current_time[20];
    TickType_t last_wake_time;
    uint8_t cnt;
    float temp, hum;
    uint16_t co2;
    ESP_LOGI(TAG, "report env process start.");

    while (true)
    {
        last_wake_time = xTaskGetTickCount();
        temp = hum = NAN;
        co2 = 0xFFFF;

        sensors_driver_read(&data);
        time_t now = time(NULL); // 秒级时间戳
        tm tm_info;
        localtime_r(&now, &tm_info); // 转本地时间
        strftime(current_day, 20, "%Y-%m-%d", &tm_info);
        strftime(current_time, 20, "%H:%M:%S", &tm_info);
        fs::path file_path = std::format("/sdcard/env/{}.txt", current_day);
        if (!fs::exists(file_path.parent_path())) [[unlikely]] // 创建父目录
            fs::create_directories(file_path.parent_path());
        std::ofstream file(file_path, std::ios::app);
        if (!file.is_open()) [[unlikely]]
        {
            ESP_LOGE(TAG, "Cannot openfile: %s", file_path.c_str());
            continue;
        }

        cnt = 0;
        do
        {
            xSemaphoreTake(i2c_mutex, portMAX_DELAY);
            sensors_driver_read(&data);
            xSemaphoreGive(i2c_mutex);
            if (data.sht_valid)
            {
                file << std::format("{} {:<6.1f} {:<6.1f}\n",
                                    current_time, data.temperature_c, data.humidity_rh);
                temp = data.temperature_c;
                hum = data.humidity_rh;
                break;
            }
            cnt++;
            vTaskDelay(pdMS_TO_TICKS(5));
        } while (cnt < 5);
        if (!data.sht_valid)
        {
            file << std::format("{} {:<6} {:<6}\n",
                                current_time, "Err", "Err");
            temp = NAN;
            hum = NAN;
        }

        // cnt = 0;
        // do
        // {
        //     sensors_driver_read(&data);
        //     if (data.sgp_valid)
        //     {
        //         file << std::format("{:<6}\n", data.eco2_ppm);
        //         co2 = data.eco2_ppm;
        //         break;
        //     }
        //     cnt++;
        //     vTaskDelay(pdMS_TO_TICKS(5));
        // } while (cnt < 5);
        // if (!data.sgp_valid)
        // {
        //     file << std::format("{:<6}\n", "Err");
        //     co2 = 0xFFFF;
        // }
        IOT::report_env(temp, hum, co2);
        file.close(); // 关闭文件

        ESP_LOGI(TAG, "Temp: %.1f, Hum: %.1f, CO2: %d", temp, hum, co2);
        ESP_LOGI(TAG, "%d", uxTaskGetStackHighWaterMark(nullptr));
        xTaskDelayUntil(&last_wake_time,
                        pdMS_TO_TICKS(args->delay.load()));
    }
}
