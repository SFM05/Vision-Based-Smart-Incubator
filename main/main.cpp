#include "cJSON.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

#include "dl_image_jpeg.hpp"

#include "ft6336_driver.hpp"
#include "inference_task/inference_task.hpp"
#include "lvgl_port/lvgl_port.hpp"
#include "sensors/sensor_task.hpp"
#include "soc/gpio_num.h"
#include "st7796_driver.hpp"
#include "ui/gui_guider.h"

extern "C"
{
#include "esp_sccb_i2c.h"
#include "esp_video_init.h"
#include "ov5647.h"
}

#include "iot/iot_utils.hpp"
#include "iot/mqtt.hpp"
#include "iot/nvs_init.hpp"
#include "iot/wifi.hpp"
#include "sdkconfig.h"

#define TEST
#include "test.hpp"

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

constexpr gpio_num_t GPIO_CAM_SDA = GPIO_NUM_7;
constexpr gpio_num_t GPIO_CAM_SCL = GPIO_NUM_8;

constexpr unsigned int CLK_FREQUENCY = 10e6;

static void upload_task(void *arg)
{
    char msg[512];
    while (true)
    {
        if (xQueueReceive(MQTT::MQTT::get_instance().m_upload_queue, msg,
                          pdMS_TO_TICKS(60000)) == pdTRUE)
        {
            ESP_LOGI(TAG, "Upload message received");

            cJSON *root = cJSON_Parse(msg);
            if (!root)
            {
                ESP_LOGW(TAG, "Failed to parse upload message");
                continue;
            }

            cJSON *url_item = cJSON_GetObjectItem(root, "url");
            if (!url_item)
            {
                ESP_LOGW(TAG, "No URL in upload message");
                cJSON_Delete(root);
                continue;
            }

            const char *url = url_item->valuestring;
            ESP_LOGI(TAG, "Uploading to: %s", url);

            uint8_t *rgb_buf;
            size_t rgb_size;
            if (inference_get_raw_snapshot(&rgb_buf, &rgb_size) != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to get raw snapshot");
                cJSON_Delete(root);
                continue;
            }

            dl::image::img_t img = {
                .data = rgb_buf,
                .width = 640,
                .height = 640,
                .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB888};
            dl::image::jpeg_img_t jpeg = dl::image::hw_encode_jpeg(img, 85);
            heap_caps_free(rgb_buf);

            if (!jpeg.data)
            {
                ESP_LOGE(TAG, "JPEG encoding failed");
                cJSON_Delete(root);
                continue;
            }

            esp_err_t err = IOT::upload_file(url, jpeg.data, jpeg.data_len);
            if (err == ESP_OK)
            {
                ESP_LOGI(TAG, "Upload successful (%zu bytes)", jpeg.data_len);
            }
            else
            {
                ESP_LOGE(TAG, "Upload failed");
            }

            free(jpeg.data);
            cJSON_Delete(root);
        }
    }
}

extern "C" void app_main(void)
{
#ifdef TEST
    iot_test();
#endif

#ifndef TEST
    ESP_LOGI(TAG, "=== Vision-Based Smart Incubator ===");
    vTaskPrioritySet(NULL, 5);

    esp_task_wdt_config_t twdt_cfg = {.timeout_ms = 30000, .idle_core_mask = 0, .trigger_panic = false};
    esp_task_wdt_init(&twdt_cfg);

    LCD::ST7796 panel{SPI2_HOST, W, H};
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
    ESP_ERROR_CHECK(init_sensor_devices(touch.i2c_handle()));
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

    start_sensor_task(&guider_ui);

    NVS_init::nvs_init();
    IOT::Wifi::get_instance().init();
    IOT::Wifi::get_instance().connect(CONFIG_IOT_WIFI_SSID, CONFIG_IOT_WIFI_PASSWORD);

    if (IOT::Wifi::get_instance().status() & IOT::WIFI_CONNECTED_BIT)
    {
        IOT::ntp_time_sync();

        MQTT::MQTT::get_instance().set_mqtt_cfg(
            CONFIG_IOT_MQTT_SERVER,
            CONFIG_IOT_MQTT_USERNAME,
            CONFIG_IOT_MQTT_PASSWORD);
        MQTT::MQTT::get_instance().mqtt_init();

        xTaskCreate(upload_task, "upload_task", 16384, NULL, 5, NULL);
        ESP_LOGI(TAG, "MQTT + upload task started");
    }

    start_inference_task(&guider_ui);
    ESP_LOGI(TAG, "Inference task started");

    while (true)
    {
        LvglPort::tick();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
#endif
}
