#include "dl_detect_define.hpp"
#include "dl_image_define.hpp"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include <cstddef>
#include <cstring>
#include <vector>

#include "dl_image_jpeg.hpp"

#include "ft6336_driver.hpp"
#include "hal/spi_types.h"
#include "inference_task/inference_task.hpp"
#include "lvgl_port/lvgl_port.hpp"
#include "sensors/sensor_task.hpp"
#include "soc/gpio_num.h"
#include "st7796_driver.hpp"
#include "ui/gui_guider.h"
#include "yolo26.hpp"

extern "C"
{
#include "esp_sccb_i2c.h"
#include "esp_video_init.h"
#include "ov5647.h"
}

#include "eth.hpp"
#include "iot/iot_utils.hpp"
#include "iot/mqtt.hpp"
#include "sdcard.hpp"
#include "sdkconfig.h"

// #define TEST
#include "process.hpp"
#include "test.hpp"

static const char *TAG = "main";

constexpr uint16_t W = 480, H = 320;

constexpr gpio_num_t GPIO_CS = GPIO_NUM_54;
constexpr gpio_num_t GPIO_RST = GPIO_NUM_19;
constexpr gpio_num_t GPIO_RS = GPIO_NUM_18; // LCD_RS (D/C)
constexpr gpio_num_t GPIO_MOSI = GPIO_NUM_16;
constexpr gpio_num_t GPIO_CLK = GPIO_NUM_15;

constexpr gpio_num_t GPIO_CTP_SCL = GPIO_NUM_14;
constexpr gpio_num_t GPIO_CTP_RST = GPIO_NUM_6;
constexpr gpio_num_t GPIO_CTP_SDA = GPIO_NUM_5;
constexpr gpio_num_t GPIO_CTP_INT = GPIO_NUM_4;

constexpr gpio_num_t GPIO_CAM_SDA = GPIO_NUM_7;
constexpr gpio_num_t GPIO_CAM_SCL = GPIO_NUM_8;

constexpr unsigned int CLK_FREQUENCY = 10e6;

extern "C" void app_main(void)
{
#ifdef TEST
    iot_test();
#endif

#ifndef TEST
    ESP_LOGI(TAG, "=== Vision-Based Smart Incubator ===");
    vTaskPrioritySet(NULL, 5);

    LCD::ST7796 panel{SPI3_HOST, W, H};
    panel.set_spi_cfg(GPIO_MOSI, GPIO_CLK);
    panel.set_io_cfg(GPIO_CS, GPIO_RS, CLK_FREQUENCY);
    panel.set_panel_cfg(GPIO_RST);
    ESP_ERROR_CHECK(panel.spi_init());
    ESP_ERROR_CHECK(panel.io_init());
    ESP_ERROR_CHECK(panel.panel_init());
    ESP_LOGI(TAG, "LCD ready");

    LCD::FT6336 touch{W, H};
    touch.set_i2c_config(GPIO_CTP_SDA, GPIO_CTP_SCL);
    touch.set_touch_config(GPIO_CTP_RST, GPIO_CTP_INT);
    ESP_ERROR_CHECK(touch.i2c_init());
    // ESP_ERROR_CHECK(init_sensor_devices(touch.i2c_handle()));
    ESP_ERROR_CHECK(touch.io_init());
    esp_err_t terr = touch.touch_init();
    if (terr != ESP_OK)
    {
        ESP_LOGW(TAG, "Touch init failed (%s), continuing", esp_err_to_name(terr));
    }
    else
    {
        ESP_LOGI(TAG, "Touch ready");
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    i2c_master_bus_handle_t cam_i2c = NULL;
    i2c_master_bus_config_t cam_bus = {};
    cam_bus.i2c_port = I2C_NUM_0;
    cam_bus.sda_io_num = GPIO_CAM_SDA;
    cam_bus.scl_io_num = GPIO_CAM_SCL;
    cam_bus.clk_source = I2C_CLK_SRC_DEFAULT;
    cam_bus.flags.enable_internal_pullup = true;

    if (i2c_new_master_bus(&cam_bus, &cam_i2c) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create camera I2C bus");
    }
    else
    {
        ESP_LOGI(TAG, "Camera I2C bus created");
    }

    if (cam_i2c)
    {
        esp_video_init_sccb_config_t sccb = {};
        sccb.init_sccb = false;
        sccb.i2c_handle = cam_i2c;
        sccb.freq = 100000;

        esp_video_init_csi_config_t csi = {};
        csi.sccb_config = sccb;
        csi.reset_pin = GPIO_NUM_NC;
        csi.pwdn_pin = GPIO_NUM_NC;

        esp_video_init_config_t video_cfg = {};
        video_cfg.csi = &csi;

        esp_err_t cam_err = esp_video_init(&video_cfg);
        if (cam_err == ESP_OK)
        {
            ESP_LOGI(TAG, "Camera ready");
        }
        else
        {
            ESP_LOGW(TAG, "Camera init failed (%s)", esp_err_to_name(cam_err));
        }
    }

    capture_init();
    configure_isp();

    ESP_ERROR_CHECK(LvglPort::init(
        panel.panel_handle(),
        panel.io_handle(),
        touch.touch_handle(),
        W, H));

    {
        MirrorConfig cfg;
        cfg.mv = true;
        cfg.mx = false;
        cfg.my = false;
        panel.set_mirror(cfg);
        LvglPort::set_mirror(cfg);
    }
    ESP_LOGI(TAG, "LVGL ready (mirror: MV=%d MX=%d MY=%d)",
             panel.mirror().mv, panel.mirror().mx, panel.mirror().my);

    setup_ui(&guider_ui);
    LvglPort::refresh();
    ESP_LOGI(TAG, "UI ready");

    // start_sensor_task(&guider_ui);

    ESP_LOGI(TAG, "%d",
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    sdcard::SDcard::instance().init();

    ETH::Ethernet::instance().init();

    if (ETH::Ethernet::instance().status())
    {
        // IOT::ntp_time_sync();

        MQTT::MQTT::get_instance().set_mqtt_cfg(
            CONFIG_IOT_MQTT_SERVER,
            CONFIG_IOT_MQTT_USERNAME,
            CONFIG_IOT_MQTT_PASSWORD);
        MQTT::MQTT::get_instance().mqtt_init();

        // ESP_LOGI(TAG, "MQTT + upload task started");
    }

    // start_inference_task(&guider_ui);

    // 启动捕捉任务
    dl::image::img_t cropped_buf{
        nullptr,
        640,
        640,
        dl::image::DL_IMAGE_PIX_TYPE_RGB565LE};
    cropped_buf.data = heap_caps_malloc(cropped_buf.bytes(),
                                        MALLOC_CAP_SPIRAM);
    SemaphoreHandle_t capture_inference_mutex = xSemaphoreCreateMutex();
    QueueHandle_t capture_inference_queue = xQueueCreate(2, sizeof(frame_meta_data));
    SemaphoreHandle_t inference_end = xSemaphoreCreateBinary();
    QueueHandle_t capture_request_queue = xQueueCreate(2, sizeof(frame_meta_data));
    capture_process_args capture_args{
        cropped_buf,
        // start_inference,
        capture_inference_queue,
        inference_end,
        capture_inference_mutex,
        capture_request_queue};
    xTaskCreate(capture_process,
                "Capture",
                4096,
                &capture_args,
                1,
                nullptr);

    // 启动推理任务
    dl::Model *model = new dl::Model("yolo26",
                                     fbs::MODEL_LOCATION_IN_FLASH_PARTITION);
    const char *names[] = {"Colony"};
    constexpr int TARGET_K = 128;
    constexpr float CONF_THRESH = 0.25f;
    YOLO26 processor = YOLO26(model, TARGET_K, CONF_THRESH, names);
    std::vector<dl::detect::result_t> result;
    result.reserve(TARGET_K);
    inference_process_args inference_args{
        cropped_buf,
        model,
        processor,
        result,
        capture_inference_queue,
        inference_end,
        capture_inference_mutex};
    xTaskCreate(inference_process,
                "Inference",
                4096,
                &inference_args,
                1,
                nullptr);

    // 启动上传请求任务
    upload_request_process_args upload_request_args{
        capture_request_queue,
        result};
    xTaskCreate(upload_request_process,
                "Upload Request",
                5120,
                &upload_request_args,
                1,
                nullptr);

    // 启动真实上传任务
    upload_process_args upload_args{
        MQTT::MQTT::get_instance().m_upload_queue};
    xTaskCreate(upload_process,
                "Upload",
                8192,
                &upload_args,
                1,
                nullptr);

    while (true)
    {
        // LvglPort::tick();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
#endif
}
