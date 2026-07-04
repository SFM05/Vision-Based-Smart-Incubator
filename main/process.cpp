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

#include "cam.hpp"
#include "iot_utils.hpp"
#include "mqtt.hpp"
#include "sensor.h"

#include "process.hpp"

static const char *TAG = "process";
static constexpr int CROP_SIZE = 640;

void capture_process(void *argv)
{
    const char *TAG = "capture";
    ESP_LOGI(TAG, "Capture process started.");
    auto args = static_cast<capture_process_args *>(argv);

    /**
     * @brief 再初始化和启动摄像头（先分配推理缓冲区会影响 ISP DMA，故摄像头启动后再分配）
     */
    // capture_init();

    // configure_isp();

    v4l2_buffer cam_buf{};
    cam_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cam_buf.memory = V4L2_MEMORY_MMAP;

    uint8_t *frame = nullptr;
    dl::image::img_t raw{
        nullptr,
        CAM::camera::instance().width(),
        CAM::camera::instance().height(),
        dl::image::DL_IMAGE_PIX_TYPE_RGB565LE};
    dl::image::ImageTransformer transformer;

    while (true)
    {
        if (!CAM::camera::instance().streaming())
        {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        // 接管帧缓冲区
        auto state = ioctl(CAM::camera::instance().fd(), VIDIOC_DQBUF, &cam_buf);
        if (state < 0) // 缓冲帧未准备完毕
            continue;
        if (cam_buf.index >= static_cast<uint32_t>(CAM::camera::instance().num_bufs()))
        {
            ioctl(CAM::camera::instance().fd(), VIDIOC_QBUF, &cam_buf);
            continue;
        }

        frame = static_cast<uint8_t *>(CAM::camera::instance().buf_start()[cam_buf.index]); // 获取当前帧
        // s_cap.last_index = cam_buf.index;
        esp_cache_msync(frame, CAM::camera::instance().buf_len()[cam_buf.index],
                        ESP_CACHE_MSYNC_FLAG_DIR_M2C); // 同步缓冲

        raw.data = static_cast<void *>(frame);
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
        strftime(data.time, 32, "%Y%m%d-%H%M%S", &tm_info);

        vTaskDelay(pdMS_TO_TICKS(1));
        // frame_meta_data data{current_time, plate_id};
        xQueueSend(args->start_inference, &data, pdMS_TO_TICKS(200));
        // xSemaphoreGive(args->start_inference); // 启动推理

        taskYIELD(); // 进入就绪态

        // 编码输出至sd卡
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
        ESP_LOGI(TAG, "Write to path: %s", file_path.c_str());

        xSemaphoreTake(args->inference_end, portMAX_DELAY);
        // 通知上传任务
        xQueueSend(args->request_upload, &data, portMAX_DELAY);
        ESP_LOGI(TAG, "%d", uxTaskGetStackHighWaterMark(nullptr));
        vTaskDelay(pdMS_TO_TICKS(30000));
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

        // xSemaphoreTake(args->capture_inference_mutex, portMAX_DELAY); // 加锁保证多线程
        // dl::image::ImageTransformer transformer;
        // transformer.set_src_img(args->cropped_buf);
        // transformer.set_dst_img(args->image);
        // transformer.transform();
        // xSemaphoreGive(args->capture_inference_mutex);

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

        // 通知上传任务
        xSemaphoreGive(args->inference_end);
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

        if (cJSON_GetObjectItem(root, "sucess")->type == cJSON_True)
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
    sensors_data_t data;
    esp_err_t err;

    while (true)
    {
        err = sensors_driver_read(&data);
        if (err == ESP_OK && data.is_valid)
        {
            IOT::report_env(data.temperature_c, data.humidity_rh, data.eco2_ppm);

            ESP_LOGI(TAG, "%s",
                     std::format("T={:>5.1f},RH={:>5.1f}",
                                 data.temperature_c, data.humidity_rh)
                         .c_str());
        }
        else
        {
            ESP_LOGW(TAG, "Read failed (SGP30= %s, SHT40= %s)",
                     data.eco2_ppm ? "OK" : "FAIL",
                     data.temperature_c != 0 ? "OK" : "FAIL");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
