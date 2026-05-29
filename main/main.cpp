#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_ops.h"
#include "esp_task_wdt.h"

#include "st7796_driver.hpp"
#include "ft6336_driver.hpp"
#include "lvgl_port/lvgl_port.hpp"
#include "ui/gui_guider.h"
#include "inference_task/inference_task.hpp"
#include "sensors/sensor_task.hpp"

/**
 * @brief Camera 相关头文件
 */
extern "C" {
#include "esp_sccb_i2c.h"
#include "ov5647.h"
#include "esp_video_init.h"
}

static const char *TAG = "main";

constexpr uint16_t W = 480, H = 320;

constexpr gpio_num_t GPIO_CS = GPIO_NUM_36;
constexpr gpio_num_t GPIO_RST = GPIO_NUM_34;
constexpr gpio_num_t GPIO_RS = GPIO_NUM_33;
constexpr gpio_num_t GPIO_MOSI = GPIO_NUM_32;
constexpr gpio_num_t GPIO_CLK = GPIO_NUM_31;

constexpr gpio_num_t GPIO_CTP_SCL = GPIO_NUM_29;
constexpr gpio_num_t GPIO_CTP_RST = GPIO_NUM_28;
constexpr gpio_num_t GPIO_CTP_SDA = GPIO_NUM_27;
constexpr gpio_num_t GPIO_CTP_INT = GPIO_NUM_26;

/**
 * @brief Camera I2C 引脚定义 (独立于触控 I2C)
 */
constexpr gpio_num_t GPIO_CAM_SDA = GPIO_NUM_7;
constexpr gpio_num_t GPIO_CAM_SCL = GPIO_NUM_8;

constexpr unsigned int CLK_FREQUENCY = 10e6;

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=== Vision-Based Smart Incubator ===");
    vTaskPrioritySet(NULL, 5);

    esp_task_wdt_config_t twdt_cfg = { .timeout_ms = 30000, .idle_core_mask = 0, .trigger_panic = false };
    esp_task_wdt_init(&twdt_cfg);

    /**
     * @brief LCD 初始化
     */
    LCD::ST7796 panel{SPI2_HOST, W, H};
    panel.set_spi_cfg(GPIO_MOSI, GPIO_CLK);
    panel.set_io_cfg(GPIO_CS, GPIO_RS, CLK_FREQUENCY);
    panel.set_panel_cfg(GPIO_RST);
    ESP_ERROR_CHECK(panel.spi_init());
    ESP_ERROR_CHECK(panel.io_init());
    ESP_ERROR_CHECK(panel.panel_init());
    ESP_LOGI(TAG, "LCD ready");

    /**
     * @brief Touch 初始化 (I2C_NUM_1)
     */
    LCD::FT6336 touch{W, H};
    touch.set_i2c_config(GPIO_CTP_SDA, GPIO_CTP_SCL);
    touch.set_touch_config(GPIO_CTP_RST, GPIO_CTP_INT);
    ESP_ERROR_CHECK(touch.i2c_init());

    /**
     * @brief 传感器 I2C 设备初始化 (必须在 touch.io_init() 之前做 I2C 事务)
     */
    ESP_ERROR_CHECK(init_sensor_devices(touch.i2c_handle()));

    ESP_ERROR_CHECK(touch.io_init());
    esp_err_t terr = touch.touch_init();
    if (terr != ESP_OK) {
        ESP_LOGW(TAG, "Touch init failed (%s), continuing", esp_err_to_name(terr));
    } else {
        ESP_LOGI(TAG, "Touch ready");
    }

    /**
     * @brief Camera 初始化
     */
    vTaskDelay(pdMS_TO_TICKS(500));

    i2c_master_bus_handle_t cam_i2c = NULL;
    i2c_master_bus_config_t cam_bus = {};
    cam_bus.i2c_port = I2C_NUM_0;
    cam_bus.sda_io_num = GPIO_CAM_SDA;
    cam_bus.scl_io_num = GPIO_CAM_SCL;
    cam_bus.clk_source = I2C_CLK_SRC_DEFAULT;
    cam_bus.flags.enable_internal_pullup = true;

    if (i2c_new_master_bus(&cam_bus, &cam_i2c) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create camera I2C bus");
    } else {
        ESP_LOGI(TAG, "Camera I2C bus created");
    }

    if (cam_i2c) {
        esp_video_init_sccb_config_t sccb = {};
        sccb.init_sccb = false;
        sccb.i2c_handle = cam_i2c;
        sccb.freq = 100000;

        esp_video_init_csi_config_t csi = {};
        csi.sccb_config = sccb;
        csi.reset_pin = -1;
        csi.pwdn_pin = -1;

        esp_video_init_config_t video_cfg = {};
        video_cfg.csi = &csi;

        esp_err_t cam_err = esp_video_init(&video_cfg);
        if (cam_err == ESP_OK) {
            ESP_LOGI(TAG, "Camera ready");
        } else {
            ESP_LOGW(TAG, "Camera init failed (%s)", esp_err_to_name(cam_err));
        }
    }

    /**
     * @brief LVGL 初始化
     */
    ESP_ERROR_CHECK(LvglPort::init(
        panel.panel_handle(),
        panel.io_handle(),
        touch.touch_handle(),
        W, H));

    /**
     * @brief 设置屏幕 X Y 双轴镜像，并同步到 LVGL 触摸映射
     *
     * MX=1: X 轴镜像（水平翻转）
     * MY=1: Y 轴镜像（垂直翻转）
     * MV=1: 横屏模式（行列交换）
     *
     * 修改镜像后必须同步触摸，否则触摸会错位。
     * 运行时修改镜像示例:
     * @code
     *   panel.set_mirror({.mv=true, .mx=true, .my=false});   // 仅 X 镜像
     *   LvglPort::set_mirror(panel.mirror());
     * @endcode
     */
    {
        MirrorConfig cfg;
        cfg.mv = true;   // 横屏
        cfg.mx = false;  // 取消 X 轴镜像
        cfg.my = false;  // 取消 Y 轴镜像
        panel.set_mirror(cfg);
        LvglPort::set_mirror(cfg);
    }
    ESP_LOGI(TAG, "LVGL ready (mirror: MV=%d MX=%d MY=%d)",
             panel.mirror().mv, panel.mirror().mx, panel.mirror().my);

    /**
     * @brief UI 初始化
     */
    setup_ui(&guider_ui);
    LvglPort::refresh();
    ESP_LOGI(TAG, "UI ready");

    /**
     * @brief 传感器任务 (SHT40 + SGP30)，使用 Touch 的 I2C_NUM_1 总线
     * 采样间隔 2s，数据通过 LVGL timer 更新到 screen2 的 Temperature/Humidity 标签
     */
    start_sensor_task(&guider_ui);

    /**
     * @brief 启动推理任务
     */
    start_inference_task(&guider_ui);
    ESP_LOGI(TAG, "Inference task started");

    /**
     * @brief 主循环 (LVGL tick)
     */
    while (true) {
        LvglPort::tick();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
