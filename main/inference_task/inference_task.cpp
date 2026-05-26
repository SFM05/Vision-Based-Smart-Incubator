#include "inference_task.hpp"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <cstring>
#include <algorithm>
#include <new>

#include "esp_video_device.h"
#include "linux/videodev2.h"
#include <sys/mman.h>

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

#include "esp_partition.h"
#include "spi_flash_mmap.h"
#include "dl_model_base.hpp"
#include "yolo26.hpp"
#include "yolo_inference.hpp"
#include "image_auto.hpp"
#include "coco_classes.hpp"



static const char *TAG = "camera_preview";
static constexpr int CROP_SIZE = 640;

static struct {
    SemaphoreHandle_t mutex;
    int  detection_count;
    bool has_new_result;
    volatile bool inferring;
    uint8_t *rgb888_buf;
    bool initialized;
} s_state = {};

static int s_interval_sec = 10;
static lv_ui *s_ui = nullptr;

static dl::Model *s_model = nullptr;
static YOLO26    *s_processor = nullptr;

/**
 * @brief V4L2 捕获模块
 */

static struct {
    int     fd;
    void  **buf_start;
    size_t *buf_len;
    int     num_bufs;
    uint16_t width;
    uint16_t height;
    uint32_t pixfmt;
    int      bpp;
    bool     streaming;
    int      last_index;
} s_cap = { -1, nullptr, nullptr, 0, 0, 0, 0, 0, false, 0 };

static bool capture_init(void)
{
    memset(&s_cap, 0, sizeof(s_cap));
    s_cap.fd = -1;

    /**
     * @brief 等待 V4L2 设备注册完成
     */
    vTaskDelay(pdMS_TO_TICKS(200));
    s_cap.fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDWR);
    if (s_cap.fd < 0) {
        ESP_LOGE(TAG, "Failed to open %s (errno=%d)", ESP_VIDEO_MIPI_CSI_DEVICE_NAME, errno);
        return false;
    }

    struct v4l2_capability cap;
    if (ioctl(s_cap.fd, VIDIOC_QUERYCAP, &cap) < 0) {
        ESP_LOGE(TAG, "VIDIOC_QUERYCAP failed");
        close(s_cap.fd); s_cap.fd = -1; return false;
    }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(s_cap.fd, VIDIOC_G_FMT, &fmt) < 0) {
        ESP_LOGE(TAG, "VIDIOC_G_FMT failed");
        close(s_cap.fd); s_cap.fd = -1; return false;
    }
    s_cap.width  = fmt.fmt.pix.width;
    s_cap.height = fmt.fmt.pix.height;
    s_cap.pixfmt = fmt.fmt.pix.pixelformat;
    ESP_LOGI(TAG, "Camera format: %dx%d pixfmt=0x%08x", s_cap.width, s_cap.height, s_cap.pixfmt);

    s_cap.bpp = (s_cap.pixfmt == V4L2_PIX_FMT_RGB24) ? 3 : 2;

    struct v4l2_requestbuffers reqbuf;
    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.count  = 2;
    reqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(s_cap.fd, VIDIOC_REQBUFS, &reqbuf) < 0) {
        ESP_LOGE(TAG, "REQBUFS failed");
        close(s_cap.fd); s_cap.fd = -1; return false;
    }

    s_cap.num_bufs = reqbuf.count;
    s_cap.buf_start = (void **)calloc(reqbuf.count, sizeof(void *));
    s_cap.buf_len   = (size_t *)calloc(reqbuf.count, sizeof(size_t));
    if (!s_cap.buf_start || !s_cap.buf_len) {
        free(s_cap.buf_start); free(s_cap.buf_len);
        close(s_cap.fd); s_cap.fd = -1; return false;
    }

    for (int i = 0; i < reqbuf.count; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (ioctl(s_cap.fd, VIDIOC_QUERYBUF, &buf) < 0) goto fail;
        s_cap.buf_start[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                   MAP_SHARED, s_cap.fd, buf.m.offset);
        if (s_cap.buf_start[i] == MAP_FAILED) goto fail;
        s_cap.buf_len[i] = buf.length;
    }

    for (int i = 0; i < reqbuf.count; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        ioctl(s_cap.fd, VIDIOC_QBUF, &buf);
    }

    {
    enum v4l2_buf_type bt = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(s_cap.fd, VIDIOC_STREAMON, &bt) < 0) goto fail;
    }
    s_cap.streaming = true;
    ESP_LOGI(TAG, "Camera started");
    return true;

fail:
    for (int i = 0; i < reqbuf.count; i++)
        if (s_cap.buf_start[i] && s_cap.buf_start[i] != MAP_FAILED)
            munmap(s_cap.buf_start[i], s_cap.buf_len[i]);
    free(s_cap.buf_start); free(s_cap.buf_len);
    close(s_cap.fd); s_cap.fd = -1;
    return false;
}

/**
 * @brief 图像裁剪与格式转换
 */

static void crop_center_rgb565_to_rgb888(
    const uint8_t *src, int src_w, int src_h,
    uint8_t *dst, int dst_w, int dst_h)
{
    int crop_x = (src_w - dst_w) / 2;
    int crop_y = (src_h - dst_h) / 2;
    if (crop_x < 0) crop_x = 0;
    if (crop_y < 0) crop_y = 0;

    for (int y = 0; y < dst_h; y++) {
        const uint16_t *src_row = (const uint16_t *)src + (crop_y + y) * src_w + crop_x;
        uint8_t *dst_row = dst + y * dst_w * 3;
        for (int x = 0; x < dst_w; x++) {
            uint16_t p = src_row[x];
            dst_row[x * 3 + 0] = ((p >> 11) & 0x1F) * 255 / 31;
            dst_row[x * 3 + 1] = ((p >> 5)  & 0x3F) * 255 / 63;
            dst_row[x * 3 + 2] = (p & 0x1F) * 255 / 31;
        }
    }
}

/**
 * @brief YOLO 模型加载与初始化
 */

static bool model_init(void)
{
    s_model = new (std::nothrow) dl::Model("yolo26", fbs::MODEL_LOCATION_IN_FLASH_PARTITION);
    if (!s_model) { ESP_LOGE(TAG, "Failed to create model"); return false; }
    s_model->build(0, dl::MEMORY_MANAGER_GREEDY, false);
    s_processor = new (std::nothrow) YOLO26(s_model, 32, 0.25f, coco_classes);
    if (!s_processor) { delete s_model; s_model = nullptr; return false; }
    ESP_LOGI(TAG, "YOLO model loaded");
    return true;
}

/**
 * @brief 绘制检测框
 */

static void draw_box_rgb888(uint8_t *img, int w, int h,
                            int x1, int y1, int x2, int y2,
                            uint8_t r, uint8_t g, uint8_t b)
{
    x1 = std::max(0, std::min(x1, w - 1));
    y1 = std::max(0, std::min(y1, h - 1));
    x2 = std::max(0, std::min(x2, w - 1));
    y2 = std::max(0, std::min(y2, h - 1));
    const int thick = 3;
    for (int t = 0; t < thick; t++) {
        int yy1 = std::min(y1 + t, h - 1);
        int yy2 = std::max(y2 - t, 0);
        int xx1 = std::min(x1 + t, w - 1);
        int xx2 = std::max(x2 - t, 0);
        for (int x = xx1; x <= xx2; x++) {
            int o1 = (yy1 * w + x) * 3; img[o1]=r; img[o1+1]=g; img[o1+2]=b;
            int o2 = (yy2 * w + x) * 3; img[o2]=r; img[o2+1]=g; img[o2+2]=b;
        }
        for (int y = yy1; y <= yy2; y++) {
            int o1 = (y * w + xx1) * 3; img[o1]=r; img[o1+1]=g; img[o1+2]=b;
            int o2 = (y * w + xx2) * 3; img[o2]=r; img[o2+1]=g; img[o2+2]=b;
        }
    }
}

/**
 * @brief LVGL 画布更新定时器
 */

static void ui_update_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!s_state.initialized || !s_state.has_new_result)
        return;
    if (xSemaphoreTake(s_state.mutex, pdMS_TO_TICKS(50)) != pdTRUE)
        return;

    int count = s_state.detection_count;
    if (!s_ui || !s_ui->screen_1 || !s_state.rgb888_buf) {
        xSemaphoreGive(s_state.mutex);
        return;
    }

    if (s_state.inferring) {
        if (s_ui->screen_1_btn_1)
            lv_obj_add_state(s_ui->screen_1_btn_1, LV_STATE_DISABLED);
        if (s_ui->screen_2_btn_1)
            lv_obj_add_state(s_ui->screen_2_btn_1, LV_STATE_DISABLED);
    } else {
        if (s_ui->screen_1_btn_1)
            lv_obj_clear_state(s_ui->screen_1_btn_1, LV_STATE_DISABLED);
        if (s_ui->screen_2_btn_1)
            lv_obj_clear_state(s_ui->screen_2_btn_1, LV_STATE_DISABLED);
    }

    char label_buf[32];
    lv_snprintf(label_buf, sizeof(label_buf), "Current\nNum: %d", count);
    if (s_ui->screen_1_label_1)
        lv_label_set_text(s_ui->screen_1_label_1, label_buf);

    const uint8_t *src = s_state.rgb888_buf;

    lv_img_dsc_t *img_dsc = lv_canvas_get_img(s_ui->screen_1_canvas_1);
    if (img_dsc && img_dsc->data) {
        uint8_t *canvas_raw = (uint8_t *)img_dsc->data;
        size_t px_stride = sizeof(lv_color_t) + 1;

        int src_x[320];
        for (int x = 0; x < 320; x++)
            src_x[x] = (x * CROP_SIZE) / 320;

        for (int y = 0; y < 320; y++) {
            int sy = (y * CROP_SIZE) / 320;
            const uint8_t *src_row = src + sy * CROP_SIZE * 3;
            size_t row_off = y * 320 * px_stride;
            for (int x = 0; x < 320; x++) {
                const uint8_t *p = src_row + src_x[x] * 3;
                lv_color_t c = lv_color_make(p[0], p[1], p[2]);
                size_t off = row_off + x * px_stride;
                memcpy(canvas_raw + off, &c, sizeof(lv_color_t));
                canvas_raw[off + sizeof(lv_color_t)] = 255;
            }
        }

        size_t canvas_sz = LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(320, 320);
        esp_cache_msync((void *)img_dsc->data, canvas_sz,
                        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    }

    lv_obj_invalidate(s_ui->screen_1_canvas_1);
    s_state.has_new_result = false;
    xSemaphoreGive(s_state.mutex);
}

/**
 * @brief 处理静态图片 (12995.jpg)
 */

static void process_static_image(uint8_t *crop_buf)
{
    const esp_partition_t *img_part = esp_partition_find_first(
        ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, "image");
    if (!img_part) { ESP_LOGE(TAG, "No image partition"); return; }

    const void *img_ptr;
    esp_partition_mmap_handle_t img_handle;
    if (esp_partition_mmap(img_part, 0, img_part->size,
                           ESP_PARTITION_MMAP_DATA, &img_ptr, &img_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mmap image partition"); return;
    }

    auto decoded = decode_jpeg((uint8_t *)img_ptr, IMAGE_SIZE);
    spi_flash_munmap(img_handle);
    if (!decoded.data_const()) { ESP_LOGE(TAG, "JPEG decode failed"); return; }

    /**
     * @brief 缩放 + 填充至 640×640
     */
    auto resized = resize(decoded, CROP_SIZE, CROP_SIZE, true);
    decoded.reset();
    if (!resized.data_const()) { ESP_LOGE(TAG, "Resize failed"); return; }

    /**
     * @brief 复制到 crop_buf
     */
    memcpy(crop_buf, resized.data_const().get(), CROP_SIZE * CROP_SIZE * 3);
    resized.reset();

    int detect_count = 0;
    if (s_model && s_processor) {
        s_state.inferring = true;

        uint8_t *inf_buf = (uint8_t *)heap_caps_malloc(CROP_SIZE * CROP_SIZE * 3, MALLOC_CAP_DEFAULT);
        if (inf_buf) {
            memcpy(inf_buf, crop_buf, CROP_SIZE * CROP_SIZE * 3);
            dl::image::img_t it = {
                .data = inf_buf,
                .width = (uint16_t)CROP_SIZE,
                .height = (uint16_t)CROP_SIZE,
                .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888
            };
            Image::ImageAuto img(it);
            auto results = yolo_inference(s_model, img, *s_processor);
            detect_count = (int)results.size();
            ESP_LOGI(TAG, "Static image: %d objects", detect_count);
            for (size_t i = 0; i < results.size(); i++) {
                auto &r = results[i];
                draw_box_rgb888(crop_buf, CROP_SIZE, CROP_SIZE,
                                r.box[0], r.box[1], r.box[2], r.box[3],
                                0, 255, 0);
            }
        }

        s_state.inferring = false;
    }

    if (xSemaphoreTake(s_state.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_state.detection_count = detect_count;
        s_state.has_new_result = true;
        xSemaphoreGive(s_state.mutex);
    }
    /**
     * @brief 显示静态图片结果 3 秒
     */
    vTaskDelay(pdMS_TO_TICKS(3000));
}

/**
 * @brief 捕获与推理主任务
 */

static void capture_task_fn(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "Capture task started");

    uint8_t *crop_buf = (uint8_t *)heap_caps_malloc(CROP_SIZE * CROP_SIZE * 3, MALLOC_CAP_DEFAULT);
    if (!crop_buf) {
        ESP_LOGE(TAG, "OOM for crop buffer");
        vTaskDelete(NULL);
        return;
    }

    if (!model_init()) {
        ESP_LOGE(TAG, "Model init failed");
        heap_caps_free(crop_buf);
        vTaskDelete(NULL);
        return;
    }

    s_state.rgb888_buf = crop_buf;
    s_state.initialized = true;

    /**
     * @brief 先处理静态图片 12995.jpg
     */
    process_static_image(crop_buf);

    /**
     * @brief 再初始化和启动摄像头（先分配推理缓冲区会影响 ISP DMA，故摄像头启动后再分配）
     */
    if (!capture_init()) {
        ESP_LOGW(TAG, "Camera init failed, running static only");
        while (true) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    int tick = 0;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (!s_cap.streaming) continue;

        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(s_cap.fd, VIDIOC_DQBUF, &buf) < 0) {
            continue;
        }
        if (buf.index >= (unsigned)s_cap.num_bufs) {
            ioctl(s_cap.fd, VIDIOC_QBUF, &buf);
            continue;
        }

        tick++;
        bool do_inference = (tick >= s_interval_sec * 20);
        if (do_inference) tick = 0;

        uint8_t *frame = (uint8_t *)s_cap.buf_start[buf.index];
        s_cap.last_index = buf.index;

        esp_cache_msync(frame, s_cap.buf_len[buf.index],
                        ESP_CACHE_MSYNC_FLAG_DIR_M2C);

        if (do_inference) {
            crop_center_rgb565_to_rgb888(frame, s_cap.width, s_cap.height,
                                          crop_buf, CROP_SIZE, CROP_SIZE);
        }

        {
            struct v4l2_buffer q;
            memset(&q, 0, sizeof(q));
            q.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            q.memory = V4L2_MEMORY_MMAP;
            q.index = s_cap.last_index;
            ioctl(s_cap.fd, VIDIOC_QBUF, &q);
        }

        if (!do_inference) continue;

        int detect_count = 0;

        if (s_model && s_processor) {
            s_state.inferring = true;

            uint8_t *inf_buf = (uint8_t *)heap_caps_malloc(CROP_SIZE * CROP_SIZE * 3, MALLOC_CAP_DEFAULT);
            if (!inf_buf) { s_state.inferring = false; continue; }
            memcpy(inf_buf, crop_buf, CROP_SIZE * CROP_SIZE * 3);

            dl::image::img_t it = {
                .data = inf_buf,
                .width = (uint16_t)CROP_SIZE,
                .height = (uint16_t)CROP_SIZE,
                .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888
            };
            Image::ImageAuto img(it);
            auto results = yolo_inference(s_model, img, *s_processor);
            detect_count = (int)results.size();
            ESP_LOGI(TAG, "Camera: %d objects", detect_count);

            for (size_t i = 0; i < results.size(); i++) {
                auto &r = results[i];
                draw_box_rgb888(crop_buf, CROP_SIZE, CROP_SIZE,
                                r.box[0], r.box[1], r.box[2], r.box[3],
                                0, 255, 0);
            }

            s_state.inferring = false;
        }

        if (xSemaphoreTake(s_state.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            s_state.detection_count = detect_count;
            s_state.has_new_result = true;
            xSemaphoreGive(s_state.mutex);
        }
    }
}

/**
 * @brief 公开 API 接口
 */

void start_inference_task(lv_ui *ui)
{
    s_ui = ui;
    s_state.mutex = xSemaphoreCreateMutex();
    if (!s_state.mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return;
    }

    lv_timer_create(ui_update_timer_cb, 100, NULL);

    BaseType_t ret = xTaskCreatePinnedToCore(
        capture_task_fn, "capture", 32768, NULL,
        configMAX_PRIORITIES - 2, NULL, 1);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create capture task");
    } else {
        ESP_LOGI(TAG, "Capture task created");
    }
}

bool inference_is_running(void)
{
    return s_state.inferring;
}

void inference_set_interval_seconds(int sec)
{
    if (sec < 1) sec = 1;
    s_interval_sec = sec;
    ESP_LOGI(TAG, "Interval set to %d seconds", sec);
}
