#include <iostream>
#include <cstring>
#include <utility>
#include <format>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

#include "driver/gpio.h"

#include "esp_partition.h"
#include "spi_flash_mmap.h"

#include "esp_heap_caps.h"
#include "esp_heap_task_info.h"

// #include "lcd_driver.h"
#include "st7796_driver.hpp"
#include "camera_driver.h"
#include "video_pipeline.h"

#include "yolo_inference.hpp"
#include "yolo26.hpp"

static const char *TAG = "main";

// LCD参数
constexpr uint16_t HORIZONRAL_RES = 480;
constexpr uint16_t VERTICAL_RES = 320;

constexpr int GPIO_CS = GPIO_NUM_22;
constexpr int GPIO_RST = GPIO_NUM_23;
constexpr int GPIO_RS = GPIO_NUM_26; // LCD_RS (D/C)
constexpr int GPIO_MOSI = GPIO_NUM_27;
constexpr int GPIO_CLK = GPIO_NUM_32;

constexpr unsigned int CLK_FREQUENCY = 80e6;

#define LCD_TEST // 启用LCD测试
// #define YOLO_TEST // 启用YOLO测试

extern "C" void app_main(void)
{
#ifdef LCD_TEST
    LCD::ST7796 panel_driver{SPI2_HOST, HORIZONRAL_RES, VERTICAL_RES};

    panel_driver.set_spi_cfg(GPIO_MOSI, GPIO_CLK);
    panel_driver.set_io_cfg(GPIO_CS, GPIO_RS,CLK_FREQUENCY);
    panel_driver.set_panel_cfg(GPIO_RST);

    panel_driver.spi_init();
    panel_driver.io_init();
    panel_driver.panel_init();

        // 测试：清屏为红色
    uint16_t *buffer = (uint16_t *)heap_caps_malloc(HORIZONRAL_RES * 10 * sizeof(uint16_t), MALLOC_CAP_DMA);

    // 主循环可以添加其他显示逻辑
    while (1)
    {
        for (int i = 0; i < VERTICAL_RES * 10; i++)
        {
            buffer[i] = 0xF800; // 红色
        }
        for (int y = 0; y < HORIZONRAL_RES; y += 10)
        {
            esp_lcd_panel_draw_bitmap(panel_driver.panel_handle(), 0, y, VERTICAL_RES, y + 10, buffer);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));

        for (int i = 0; i < VERTICAL_RES * 10; i++)
        {
            buffer[i] = 0x07E0; // 绿色
        }
        for (int y = 0; y < HORIZONRAL_RES; y += 10)
        {
            esp_lcd_panel_draw_bitmap(panel_driver.panel_handle(), 0, y, VERTICAL_RES, y + 10, buffer);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));

        for (int i = 0; i < VERTICAL_RES * 10; i++)
        {
            buffer[i] = 0x001F; // 蓝色
        }
        for (int y = 0; y < HORIZONRAL_RES; y += 10)
        {
            esp_lcd_panel_draw_bitmap(panel_driver.panel_handle(), 0, y, VERTICAL_RES, y + 10, buffer);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // /* ---- 模块 2/3: 初始化摄像头传感器 ---- */
    // camera_driver_ctx_t camera_ctx;
    // memset(&camera_ctx, 0, sizeof(camera_ctx));

    // ret = camera_ov5647_init(&camera_ctx);
    // if (ret != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "[2/3] Camera initialization failed: %s", esp_err_to_name(ret));
    //     lcd_ek79007_deinit(panel_handle);
    //     return;
    // }
    // ESP_LOGI(TAG, "[2/3] Camera module initialized");

    // /*
    //  * ---- 模块 3/3: 初始化视频管道 ----
    //  *
    //  * @note pipeline_ctx 必须声明为 static:
    //  *       app_main() 返回后，preview_task 仍在运行并访问此结构体。
    //  *       如果使用栈变量，返回后栈帧被复用会导致数据损坏。
    //  */
    // static video_pipeline_ctx_t pipeline_ctx;
    // memset(&pipeline_ctx, 0, sizeof(pipeline_ctx));

    // video_pipeline_config_t pipeline_config = {
    //     .width = DISPLAY_WIDTH,
    //     .height = DISPLAY_HEIGHT,
    //     .num_buffers = NUM_FRAME_BUFFERS,
    // };

    // ret = video_pipeline_init(&pipeline_config, panel_handle, &pipeline_ctx);
    // if (ret != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "[3/3] Video pipeline init failed: %s", esp_err_to_name(ret));
    //     video_pipeline_deinit(&pipeline_ctx); /* 安全: 内部检查 NULL */
    //     camera_ov5647_deinit(&camera_ctx);
    //     lcd_ek79007_deinit(panel_handle);
    //     return;
    // }
    // ESP_LOGI(TAG, "[3/3] Video pipeline initialized");

    // /* ---- 启动视频流采集 ---- */
    // ret = video_pipeline_start(&pipeline_ctx);
    // if (ret != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "Failed to start video stream: %s", esp_err_to_name(ret));
    //     video_pipeline_deinit(&pipeline_ctx);
    //     camera_ov5647_deinit(&camera_ctx);
    //     lcd_ek79007_deinit(panel_handle);
    //     return;
    // }

    // ESP_LOGI(TAG, "All modules ready! Starting preview...");

    // /* ---- 创建预览任务 (Core 0, 高优先级) ---- */
    // xTaskCreatePinnedToCore(
    //     video_preview_task, /* 任务入口函数 */
    //     "preview_task",     /* 任务名称 */
    //     8192,               /* 栈大小 (8KB, 足够缩放运算) */
    //     &pipeline_ctx,      /* 参数: 管道上下文指针 */
    //     10,                 /* 优先级 (高，保证实时性) */
    //     NULL,               /* 任务句柄 (不需要外部控制) */
    //     0                   /* 绑定到 Core 0 */
    // );

    // ESP_LOGI(TAG, "System running - Real-time camera preview on Core 0!");
    // ESP_LOGI(TAG, "Pipeline: Camera -> V4L2 DQBUF -> Scale -> LCD DMA -> QBUF");
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

    ESP_LOGI(TAG, "%s", std::format("Detected {} colonies", results.size()).c_str());

    delete model;

    heap_caps_check_integrity_all(true);
    TaskHandle_t xHandle = xTaskGetCurrentTaskHandle();
    heap_caps_print_single_task_stat_overview(NULL, xHandle);
#endif
}
