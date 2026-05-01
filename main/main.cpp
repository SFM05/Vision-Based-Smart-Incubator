/**
 * @file main.cpp
 * @brief Camera LCD Demo - 应用程序入口
 *
 * 系统架构（模块化解耦）:
 *   LCD (EK79007) <---> Video Pipeline (V4L2) <---> Camera (OV5647)
 *
 * 硬件平台: ESP32-P4
 * 摄像头:    OV5647 (MIPI CSI, 1920x1080 RAW10 -> ISP RGB565, 30fps)
 * 显示屏:    EK79007 (MIPI DSI, 1024x600 RGB565)
 * 框架:      esp_video V4L2 + FreeRTOS
 */

#include <iostream>
#include <cstring>
#include <utility>
#include <format>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_partition.h"
#include "spi_flash_mmap.h"

#include "lcd_driver.h"
#include "camera_driver.h"
#include "video_pipeline.h"

#include "yolo_inference.hpp"
#include "yolo26.hpp"
#include "esp_heap_caps.h"
#include "esp_heap_task_info.h"

static const char *TAG = "main";

/* ---------- 系统参数 ---------- */

#define DISPLAY_WIDTH 1024  ///< LCD 显示宽度
#define DISPLAY_HEIGHT 600  ///< LCD 显示高度
#define NUM_FRAME_BUFFERS 2 ///< V4L2 帧缓冲数量

// #define LCD_TEST // 启用LCD测试
#define YOLO_TEST // 启用YOLO测试

// YOLO模型文件
extern const uint8_t yolo26n_inference_espdl[] asm("_binary_yolo26n_640_espdl_start");

// 测试图像
extern const uint8_t img_start[] asm("_binary_12995_jpg_start");
extern const uint8_t img_end[] asm("_binary_12995_jpg_end");

// #define LCD_TEST // 启用LCD测试
#define YOLO_TEST // 启用YOLO测试

extern "C" void app_main(void)
{
#ifdef LCD_TEST
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Camera LCD Demo - Modular Architecture");
    ESP_LOGI(TAG, "  Target: ESP32-P4");
    ESP_LOGI(TAG, "  Display: EK79007 %dx%d (MIPI DSI)", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    ESP_LOGI(TAG, "  Camera: OV5647 (MIPI CSI, 1920x1080)");
    ESP_LOGI(TAG, "  Framework: esp_video (V4L2) + FreeRTOS");
    ESP_LOGI(TAG, "========================================");

    esp_err_t ret;

    /* ---- 模块 1/3: 初始化 LCD 显示屏 ---- */
    esp_lcd_panel_handle_t panel_handle = NULL;
    ret = lcd_ek79007_init(&panel_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "[1/3] LCD initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "[1/3] LCD module initialized");

    /* ---- 模块 2/3: 初始化摄像头传感器 ---- */
    camera_driver_ctx_t camera_ctx;
    memset(&camera_ctx, 0, sizeof(camera_ctx));

    ret = camera_ov5647_init(&camera_ctx);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "[2/3] Camera initialization failed: %s", esp_err_to_name(ret));
        lcd_ek79007_deinit(panel_handle);
        return;
    }
    ESP_LOGI(TAG, "[2/3] Camera module initialized");

    /*
     * ---- 模块 3/3: 初始化视频管道 ----
     *
     * @note pipeline_ctx 必须声明为 static:
     *       app_main() 返回后，preview_task 仍在运行并访问此结构体。
     *       如果使用栈变量，返回后栈帧被复用会导致数据损坏。
     */
    static video_pipeline_ctx_t pipeline_ctx;
    memset(&pipeline_ctx, 0, sizeof(pipeline_ctx));

    video_pipeline_config_t pipeline_config = {
        .width = DISPLAY_WIDTH,
        .height = DISPLAY_HEIGHT,
        .num_buffers = NUM_FRAME_BUFFERS,
    };

    ret = video_pipeline_init(&pipeline_config, panel_handle, &pipeline_ctx);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "[3/3] Video pipeline init failed: %s", esp_err_to_name(ret));
        video_pipeline_deinit(&pipeline_ctx); /* 安全: 内部检查 NULL */
        camera_ov5647_deinit(&camera_ctx);
        lcd_ek79007_deinit(panel_handle);
        return;
    }
    ESP_LOGI(TAG, "[3/3] Video pipeline initialized");

    /* ---- 启动视频流采集 ---- */
    ret = video_pipeline_start(&pipeline_ctx);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start video stream: %s", esp_err_to_name(ret));
        video_pipeline_deinit(&pipeline_ctx);
        camera_ov5647_deinit(&camera_ctx);
        lcd_ek79007_deinit(panel_handle);
        return;
    }

    ESP_LOGI(TAG, "All modules ready! Starting preview...");

    /* ---- 创建预览任务 (Core 0, 高优先级) ---- */
    xTaskCreatePinnedToCore(
        video_preview_task, /* 任务入口函数 */
        "preview_task",     /* 任务名称 */
        8192,               /* 栈大小 (8KB, 足够缩放运算) */
        &pipeline_ctx,      /* 参数: 管道上下文指针 */
        10,                 /* 优先级 (高，保证实时性) */
        NULL,               /* 任务句柄 (不需要外部控制) */
        0                   /* 绑定到 Core 0 */
    );

    ESP_LOGI(TAG, "System running - Real-time camera preview on Core 0!");
    ESP_LOGI(TAG, "Pipeline: Camera -> V4L2 DQBUF -> Scale -> LCD DMA -> QBUF");
#endif

#ifdef YOLO_TEST
    // 从分区表中读取图像的位置
    const esp_partition_t *img_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_ANY,
        ESP_PARTITION_SUBTYPE_ANY,
        "image");

    // 将分区映射到数据地址空间
    const void *img_ptr;
    esp_partition_mmap_handle_t img_handle;

    esp_err_t err = esp_partition_mmap(
        img_partition,
        0,                       // 分区内偏移量（从头开始）
        img_partition->size,     // 映射大小
        ESP_PARTITION_MMAP_DATA, // 映射到数据空间
        &img_ptr,                // 得到的指针
        &img_handle              // 映射句柄，用于后续释放
    );

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to map partition to data memory");
        return;
    }
    ESP_LOGI(TAG, "Image mapped to data memory");

    // 解码
    auto img = decode_jpeg((uint8_t *)img_ptr, IMAGE_SIZE);
    spi_flash_munmap(img_handle); // 释放映射
    ESP_LOGI(TAG, "Image decoded");

    int center_x = img.width() / 2;
    int center_y = img.height() / 2;
    int img_size = std::max(img.width(), img.height());
    int left = std::max(0, center_x - img_size / 2);
    int right = std::min(int(img.width()), center_x + img_size / 2);
    int top = std::max(0, center_y - img_size / 2);
    int bottom = std::min(int(img.height()), center_y + img_size / 2);

    // 裁剪图像的中心部分
    auto img_cropped = crop(img, {left, top, right, bottom});
    // auto img_cropped = crop(img, 0, 0, 640, 640);
    img.reset();
    ESP_LOGI(TAG, "Image cropped");

    // 缩放至640*640
    auto img_resized = resize(img_cropped, 640, 640, true);
    // auto img_resized = img_cropped;
    // auto img_resized = resize(img_cropped, 640, 320,
    //                           {0, 0, 160, 160});
    img_cropped.reset();
    ESP_LOGI(TAG, "Image resized");

    // 加载模型
    dl::Model *model = new dl::Model("yolo26",
                                     fbs::MODEL_LOCATION_IN_FLASH_PARTITION);

    ESP_LOGI(TAG, "Model loaded");

    // 初始化处理器
    const char *names[] = {"Colony"};
    constexpr int TARGET_K = 128;
    constexpr float CONF_THRESH = 0.25f;
    YOLO26 processor = YOLO26(model, TARGET_K, CONF_THRESH, names);
    vTaskDelay(100);

    // 推理
    auto results = yolo_inference(model, img_resized, processor);
    vTaskDelay(100);

    ESP_LOGI(TAG, "Total Colonies: %u", (unsigned)results.size());

    delete model;

    heap_caps_check_integrity_all(true);
    TaskHandle_t xHandle = xTaskGetCurrentTaskHandle();
    heap_caps_print_single_task_stat_overview(NULL, xHandle);
#endif
}
