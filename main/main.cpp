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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

#include "lcd_driver.h"
#include "camera_driver.h"
#include "video_pipeline.h"

#include "yolo_inference.hpp"
#include "classes.hpp"

static const char *TAG = "main";

/* ---------- 系统参数 ---------- */

#define DISPLAY_WIDTH       1024    ///< LCD 显示宽度
#define DISPLAY_HEIGHT      600     ///< LCD 显示高度
#define NUM_FRAME_BUFFERS   2       ///< V4L2 帧缓冲数量

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
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[1/3] LCD initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "[1/3] LCD module initialized");

    /* ---- 模块 2/3: 初始化摄像头传感器 ---- */
    camera_driver_ctx_t camera_ctx;
    memset(&camera_ctx, 0, sizeof(camera_ctx));

    ret = camera_ov5647_init(&camera_ctx);
    if (ret != ESP_OK) {
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
        .width        = DISPLAY_WIDTH,
        .height       = DISPLAY_HEIGHT,
        .num_buffers  = NUM_FRAME_BUFFERS,
    };

    ret = video_pipeline_init(&pipeline_config, panel_handle, &pipeline_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[3/3] Video pipeline init failed: %s", esp_err_to_name(ret));
        video_pipeline_deinit(&pipeline_ctx);  /* 安全: 内部检查 NULL */
        camera_ov5647_deinit(&camera_ctx);
        lcd_ek79007_deinit(panel_handle);
        return;
    }
    ESP_LOGI(TAG, "[3/3] Video pipeline initialized");

    /* ---- 启动视频流采集 ---- */
    ret = video_pipeline_start(&pipeline_ctx);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start video stream: %s", esp_err_to_name(ret));
        video_pipeline_deinit(&pipeline_ctx);
        camera_ov5647_deinit(&camera_ctx);
        lcd_ek79007_deinit(panel_handle);
        return;
    }

    ESP_LOGI(TAG, "All modules ready! Starting preview...");

    /* ---- 创建预览任务 (Core 0, 高优先级) ---- */
    xTaskCreatePinnedToCore(
        video_preview_task,     /* 任务入口函数 */
        "preview_task",         /* 任务名称 */
        8192,                   /* 栈大小 (8KB, 足够缩放运算) */
        &pipeline_ctx,          /* 参数: 管道上下文指针 */
        10,                     /* 优先级 (高，保证实时性) */
        NULL,                   /* 任务句柄 (不需要外部控制) */
        0                       /* 绑定到 Core 0 */
    );

    ESP_LOGI(TAG, "System running - Real-time camera preview on Core 0!");
    ESP_LOGI(TAG, "Pipeline: Camera -> V4L2 DQBUF -> Scale -> LCD DMA -> QBUF");
#endif

#ifdef YOLO_TEST
    printf("\n");
    printf("===YOLOv26n Inference Test===\n");

    auto img = decode_jpeg(img_start, size_t(img_end - img_start));

    int center_x = img.width / 2;
    int center_y = img.height / 2;
    int img_size = std::max(img.width, img.height);
    int left = std::max(0, center_x - img_size / 2);
    int right = std::min(int(img.width), center_x + img_size / 2);
    int top = std::max(0, center_y - img_size / 2);
    int bottom = std::min(int(img.height), center_y + img_size / 2);

    // 裁剪图像的中心部分
    img = crop(img, left, top, right, bottom);

    // 缩放至640*640
    img = resize(img, 640, 640);

    // 加载模型
    dl::Model *model = new dl::Model((const char *)yolo26n_inference_espdl,
                                     fbs::MODEL_LOCATION_IN_FLASH_RODATA);

    // 初始化处理器
    Yolo26Processor processor(128, 0.1f, names);

    // 进行推理
    auto result = yolo_inference(model, img, processor);

    printf("Total Colonies: %u", (unsigned)result.size());
    delete model;
    printf("\n=== Test Complete ===\n");
#endif
}
