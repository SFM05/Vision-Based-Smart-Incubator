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

// Camera
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

// Camera I2C (独立于触控)
constexpr gpio_num_t GPIO_CAM_SDA = GPIO_NUM_7;
constexpr gpio_num_t GPIO_CAM_SCL = GPIO_NUM_8;

constexpr unsigned int CLK_FREQUENCY = 10e6;

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=== Vision-Based Smart Incubator ===");
    esp_task_wdt_config_t twdt_cfg = { .timeout_ms = 30000, .idle_core_mask = 0, .trigger_panic = false };
    esp_task_wdt_init(&twdt_cfg);

    /* ---------- LCD ---------- */
    LCD::ST7796 panel{SPI2_HOST, W, H};
    panel.set_spi_cfg(GPIO_MOSI, GPIO_CLK);
    panel.set_io_cfg(GPIO_CS, GPIO_RS, CLK_FREQUENCY);
    panel.set_panel_cfg(GPIO_RST);
    ESP_ERROR_CHECK(panel.spi_init());
    ESP_ERROR_CHECK(panel.io_init());
    ESP_ERROR_CHECK(panel.panel_init());
    esp_lcd_panel_swap_xy(panel.panel_handle(), 1);
    esp_lcd_panel_mirror(panel.panel_handle(), 1, 1);
    ESP_LOGI(TAG, "LCD ready");

    /* ---------- Touch (I2C_NUM_1) ---------- */
    LCD::FT6336 touch{W, H};
    touch.set_i2c_config(GPIO_CTP_SDA, GPIO_CTP_SCL);
    touch.set_touch_config(GPIO_CTP_RST, GPIO_CTP_INT);
    ESP_ERROR_CHECK(touch.i2c_init());
    ESP_ERROR_CHECK(touch.io_init());
    esp_err_t terr = touch.touch_init();
    if (terr != ESP_OK) {
        ESP_LOGW(TAG, "Touch init failed (%s), continuing", esp_err_to_name(terr));
    } else {
        ESP_LOGI(TAG, "Touch ready");
    }

    /* ---------- Camera ---------- */
    vTaskDelay(pdMS_TO_TICKS(500));

    // 1. 创建 I2C_NUM_0 (GPIO 7/8) — 框架自动探测已禁用, 总线空闲
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

    /* ---------- LVGL ---------- */
    ESP_ERROR_CHECK(LvglPort::init(
        panel.panel_handle(),
        panel.io_handle(),
        touch.touch_handle(),
        W, H));
    ESP_LOGI(TAG, "LVGL ready");

    /* ---------- UI ---------- */
    setup_ui(&guider_ui);
    LvglPort::refresh();
    ESP_LOGI(TAG, "UI ready");

    /* ---------- Capture Task ---------- */
    start_inference_task(&guider_ui);
    ESP_LOGI(TAG, "Capture task started");

    /* ---------- Main loop ---------- */
    while (true) {
        LvglPort::tick();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
