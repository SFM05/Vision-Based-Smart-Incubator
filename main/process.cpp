#include "cJSON.h"
#include "dl_image_define.hpp"
#include "dl_image_jpeg.hpp"
#include "dl_image_process.hpp"
#include "esp_cache.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "inference_task.hpp"
#include "iot_utils.hpp"
#include "sensor.h"

#include <cstddef>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
namespace fs = std::filesystem;

#include "esp_video_device.h"
#include "esp_video_isp_ioctl.h"
#include "linux/videodev2.h"
#include "portmacro.h"
#include <fstream>
#include <sys/mman.h>
#include <vector>

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

#include "dl_model_base.hpp"
#include "mqtt.hpp"
#include "yolo26.hpp"

#include "process.hpp"
#include <cstdint>

static const char *TAG = "process";
static constexpr int CROP_SIZE = 640;

static struct
{
    SemaphoreHandle_t mutex;
    int detection_count;
    bool has_new_result;
    volatile bool inferring;
    uint8_t *rgb888_buf;
    bool initialized;
} s_state = {};

static struct
{
    SemaphoreHandle_t mutex;
    uint8_t *buf;
} s_raw = {};

static int s_interval_sec = 10;
static lv_ui *s_ui = nullptr;

static dl::Model *s_model = nullptr;
static YOLO26 *s_processor = nullptr;

/**
 * @brief V4L2 捕获模块
 */

static struct
{
    int fd;
    void **buf_start;
    size_t *buf_len;
    int num_bufs;
    uint16_t width;
    uint16_t height;
    uint32_t pixfmt;
    int bpp;
    bool streaming;
    int last_index;
} s_cap = {-1, nullptr, nullptr, 0, 0, 0, 0, 0, false, 0};

bool capture_init()
{
    memset(&s_cap, 0, sizeof(s_cap));
    s_cap.fd = -1;

    /**
     * @brief 等待 V4L2 设备注册完成
     */
    vTaskDelay(pdMS_TO_TICKS(200));
    s_cap.fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDWR);
    if (s_cap.fd < 0)
    {
        ESP_LOGE(TAG, "Failed to open %s (errno=%d)", ESP_VIDEO_MIPI_CSI_DEVICE_NAME, errno);
        return false;
    }

    struct v4l2_capability cap;
    if (ioctl(s_cap.fd, VIDIOC_QUERYCAP, &cap) < 0)
    {
        ESP_LOGE(TAG, "VIDIOC_QUERYCAP failed");
        close(s_cap.fd);
        s_cap.fd = -1;
        return false;
    }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(s_cap.fd, VIDIOC_G_FMT, &fmt) < 0)
    {
        ESP_LOGE(TAG, "VIDIOC_G_FMT failed");
        close(s_cap.fd);
        s_cap.fd = -1;
        return false;
    }
    s_cap.width = fmt.fmt.pix.width;
    s_cap.height = fmt.fmt.pix.height;
    s_cap.pixfmt = fmt.fmt.pix.pixelformat;
    ESP_LOGI(TAG, "Camera format: %dx%d pixfmt=0x%08x", s_cap.width, s_cap.height, s_cap.pixfmt);

    s_cap.bpp = (s_cap.pixfmt == V4L2_PIX_FMT_RGB24) ? 3 : 2;

    struct v4l2_requestbuffers reqbuf;
    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.count = 2;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(s_cap.fd, VIDIOC_REQBUFS, &reqbuf) < 0)
    {
        ESP_LOGE(TAG, "REQBUFS failed");
        close(s_cap.fd);
        s_cap.fd = -1;
        return false;
    }

    s_cap.num_bufs = reqbuf.count;
    s_cap.buf_start = (void **)calloc(reqbuf.count, sizeof(void *));
    s_cap.buf_len = (size_t *)calloc(reqbuf.count, sizeof(size_t));
    if (!s_cap.buf_start || !s_cap.buf_len)
    {
        free(s_cap.buf_start);
        free(s_cap.buf_len);
        close(s_cap.fd);
        s_cap.fd = -1;
        return false;
    }

    for (int i = 0; i < reqbuf.count; i++)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (ioctl(s_cap.fd, VIDIOC_QUERYBUF, &buf) < 0)
            goto fail;
        s_cap.buf_start[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, s_cap.fd, buf.m.offset);
        if (s_cap.buf_start[i] == MAP_FAILED)
            goto fail;
        s_cap.buf_len[i] = buf.length;
    }

    for (int i = 0; i < reqbuf.count; i++)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        ioctl(s_cap.fd, VIDIOC_QBUF, &buf);
    }

    {
        enum v4l2_buf_type bt = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(s_cap.fd, VIDIOC_STREAMON, &bt) < 0)
            goto fail;
    }
    s_cap.streaming = true;
    ESP_LOGI(TAG, "Camera started");
    return true;

fail:
    for (int i = 0; i < reqbuf.count; i++)
        if (s_cap.buf_start[i] && s_cap.buf_start[i] != MAP_FAILED)
            munmap(s_cap.buf_start[i], s_cap.buf_len[i]);
    free(s_cap.buf_start);
    free(s_cap.buf_len);
    close(s_cap.fd);
    s_cap.fd = -1;
    return false;
}

/**
 * @brief 配置 ISP Bayer 去噪
 */

void configure_isp()
{
    int fd = open(ESP_VIDEO_ISP1_DEVICE_NAME, O_RDWR);
    if (fd < 0)
    {
        ESP_LOGW(TAG, "Cannot open ISP device %s: %s",
                 ESP_VIDEO_ISP1_DEVICE_NAME, strerror(errno));
        return;
    }

    struct v4l2_ext_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));

    struct v4l2_ext_controls ctrls;
    memset(&ctrls, 0, sizeof(ctrls));
    ctrls.count = 1;
    ctrls.controls = &ctrl;

    esp_video_isp_bf_t bf;
    memset(&bf, 0, sizeof(bf));
    bf.enable = true;
    bf.level = 15;
    bf.matrix[0][0] = 1;
    bf.matrix[0][1] = 2;
    bf.matrix[0][2] = 1;
    bf.matrix[1][0] = 2;
    bf.matrix[1][1] = 4;
    bf.matrix[1][2] = 2;
    bf.matrix[2][0] = 1;
    bf.matrix[2][1] = 2;
    bf.matrix[2][2] = 1;

    ctrl.id = V4L2_CID_USER_ESP_ISP_BF;
    ctrl.size = sizeof(bf);
    ctrl.p_u8 = (__u8 *)&bf;

    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls) < 0)
        ESP_LOGW(TAG, "ISP BF failed: %s", strerror(errno));
    else
        ESP_LOGI(TAG, "ISP BF level=%d", bf.level);

    // esp_isp_crop_config_t crop_cfg={
    //     {{420,0},{1499,1079}}
    // };
    // esp_isp_crop_configure(isp_proc_handle_t isp_proc, const esp_isp_crop_config_t *config)

    close(fd);
}

/**
 * @brief 图像裁剪与格式转换
 */

// static void crop_center_rgb565_to_rgb888(
//     const uint8_t *src, int src_w, int src_h,
//     uint8_t *dst, int dst_w, int dst_h)
// {
//     int crop_x = (src_w - dst_w) / 2;
//     int crop_y = (src_h - dst_h) / 2;
//     if (crop_x < 0)
//         crop_x = 0;
//     if (crop_y < 0)
//         crop_y = 0;

//     for (int y = 0; y < dst_h; y++)
//     {
//         const uint16_t *src_row = (const uint16_t *)src + (crop_y + y) * src_w + crop_x;
//         uint8_t *dst_row = dst + y * dst_w * 3;
//         for (int x = 0; x < dst_w; x++)
//         {
//             uint16_t p = src_row[x];
//             dst_row[x * 3 + 0] = ((p >> 11) & 0x1F) * 255 / 31;
//             dst_row[x * 3 + 1] = ((p >> 5) & 0x3F) * 255 / 63;
//             dst_row[x * 3 + 2] = (p & 0x1F) * 255 / 31;
//         }
//     }
// }

void capture_process(void *argv)
{
    const char *TAG = "capture";
    ESP_LOGI(TAG, "Capture process started.");
    auto args = static_cast<capture_process_args *>(argv);

    // uint8_t *crop_buf = (uint8_t *)heap_caps_malloc(CROP_SIZE * CROP_SIZE * 3, MALLOC_CAP_DEFAULT);
    // if (!crop_buf)
    // {
    //     ESP_LOGE(TAG, "OOM for crop buffer");
    //     vTaskDelete(NULL);
    //     return;
    // }

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
        s_cap.width,
        s_cap.height,
        dl::image::DL_IMAGE_PIX_TYPE_RGB565LE};
    dl::image::ImageTransformer transformer;

    while (true)
    {
        if (!s_cap.streaming)
        {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        // 接管帧缓冲区
        auto state = ioctl(s_cap.fd, VIDIOC_DQBUF, &cam_buf);
        if (state < 0) // 缓冲帧未准备完毕
            continue;
        if (cam_buf.index >= static_cast<uint32_t>(s_cap.num_bufs))
        {
            ioctl(s_cap.fd, VIDIOC_QBUF, &cam_buf);
            continue;
        }

        frame = static_cast<uint8_t *>(s_cap.buf_start[cam_buf.index]); // 获取当前帧
        s_cap.last_index = cam_buf.index;
        esp_cache_msync(frame, s_cap.buf_len[cam_buf.index],
                        ESP_CACHE_MSYNC_FLAG_DIR_M2C); // 同步缓冲

        raw.data = static_cast<void *>(frame);
        transformer.set_src_img(raw);
        transformer.set_dst_img(args->cropped_buf);
        transformer.set_src_img_crop_area({420, 0, 1499, 1079});
        transformer.transform();
        ioctl(s_cap.fd, VIDIOC_QBUF, &cam_buf); // 归还帧缓冲区
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
