#include "dl_detect_define.hpp"
#include "dl_image_define.hpp"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "led.hpp"
#include "led_strip.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "ft6336_driver.hpp"
#include "hal/gpio_types.h"
#include "hal/lv_hal_disp.h"
#include "hal/spi_types.h"
// #include "sensor.h"
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

#include "backend.hpp"
#include "cam.hpp"
#include "custom.hpp"
#include "frontend.hpp"
#include "tcd/tcd.h"
#include "sht3x.hpp"

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

constexpr gpio_num_t GPIO_SHT3x_TX=GPIO_NUM_20;
constexpr gpio_num_t GPIO_SHT3x_RX=GPIO_NUM_21;

constexpr gpio_num_t GPIO_TCD_HEATER = GPIO_NUM_22; // 加热片 PWM
constexpr gpio_num_t GPIO_LED = GPIO_NUM_23;

constexpr unsigned int CLK_FREQUENCY = 10e6;

struct mqtt_event_handler_args
{
    QueueHandle_t &upload_feedback_queue;
};
void mqtt_event_handler(void *argv,
                        esp_event_base_t base,
                        int32_t event_id,
                        void *event_data);

static SemaphoreHandle_t touch_sem;
void IRAM_ATTR touch_isr(esp_lcd_touch_s *arg)
{
    BaseType_t woken = pdFALSE;
    gpio_intr_disable(GPIO_CTP_INT);
    xSemaphoreGiveFromISR(touch_sem, &woken);
    if (woken)
        portYIELD_FROM_ISR();
}
void touch_task(void *argv);

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=== Vision-Based Smart Incubator ===");
    vTaskPrioritySet(NULL, 5);

    sensor::SHT3x::instance().set_cfg(GPIO_SHT3x_TX, GPIO_SHT3x_RX);
    sensor::SHT3x::instance().init();

    LCD::ST7796 panel{SPI3_HOST, W, H};
    panel.set_spi_cfg(GPIO_MOSI, GPIO_CLK);
    panel.set_io_cfg(GPIO_CS, GPIO_RS, CLK_FREQUENCY);
    panel.set_panel_cfg(GPIO_RST);
    ESP_ERROR_CHECK(panel.spi_init());
    ESP_ERROR_CHECK(panel.io_init());
    ESP_ERROR_CHECK(panel.panel_init());
    panel.set_swap(true);
    ESP_LOGI(TAG, "LCD ready");

    // touch_sem = xSemaphoreCreateBinary();
    // xTaskCreate(touch_task,
    //             "Touch",
    //             6144,
    //             nullptr,
    //             4,
    //             nullptr);
    LCD::FT6336 touch{W, H, 100 * 1000};
    touch.set_i2c_config(GPIO_CTP_SDA, GPIO_CTP_SCL);
    touch.set_touch_config(GPIO_CTP_RST,
                           GPIO_CTP_INT,
                           nullptr,
                           true,
                           false,
                           true);
    ESP_ERROR_CHECK(touch.i2c_init());
    // sensors_driver_init(touch.i2c_handle());
    ESP_ERROR_CHECK(touch.io_init());
    esp_err_t terr = touch.touch_init();
    i2c_mutex_init();
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

    LED::init(GPIO_LED);
    LED::off();

    tcd_start(GPIO_TCD_HEATER);
    // ESP_LOGI(TAG, "TCD Control Started");

    CAM::camera::instance().init_camera();

    sdcard::SDcard::instance().init();

    ETH::Ethernet::instance().init();

    QueueHandle_t upload_feed_back_queue = xQueueCreate(5, sizeof(char[512]));
    mqtt_event_handler_args mqtt_event_args{upload_feed_back_queue};
    if (ETH::Ethernet::instance().status())
    {
        IOT::ntp_time_sync();

        MQTT::MQTT::instance().set_mqtt_cfg(CONFIG_IOT_MQTT_SERVER,
                                            CONFIG_IOT_MQTT_USERNAME,
                                            CONFIG_IOT_MQTT_PASSWORD);
        MQTT::MQTT::instance().mqtt_init(mqtt_event_handler,
                                         &mqtt_event_args);
    }

    // 启动捕捉任务
    static dl::image::img_t cropped_buf{
        nullptr,
        1080,
        1080,
        dl::image::DL_IMAGE_PIX_TYPE_RGB565LE};
    cropped_buf.data = heap_caps_aligned_alloc(16,
                                               cropped_buf.bytes(),
                                               MALLOC_CAP_SPIRAM);
    static SemaphoreHandle_t capture_inference_mutex = xSemaphoreCreateMutex();
    static QueueHandle_t capture_inference_queue = xQueueCreate(2, sizeof(frame_meta_data));
    static SemaphoreHandle_t inference_end = xSemaphoreCreateBinary();
    static QueueHandle_t capture_request_queue = xQueueCreate(2, sizeof(frame_meta_data));
    static SemaphoreHandle_t flush_screen = xSemaphoreCreateBinary();
    static std::atomic<uint32_t> capture_delay;
    capture_delay.store(10000);
    static std::atomic<bool> running = false;
    static TaskHandle_t capture_handle = nullptr;
    static capture_process_args capture_args{
        cropped_buf,
        capture_inference_queue,
        inference_end,
        capture_inference_mutex,
        capture_request_queue,
        flush_screen,
        capture_delay,
        running};
    xTaskCreate(capture_process,
                "Capture",
                4096,
                &capture_args,
                1,
                &capture_handle);

    // 启动推理任务
    static dl::Model *model = new dl::Model("yolo26",
                                            fbs::MODEL_LOCATION_IN_FLASH_PARTITION);
    static const char *names[] = {"Colony"};
    constexpr int TARGET_K = 128;
    constexpr float CONF_THRESH = 0.25f;
    static YOLO26 processor = YOLO26(model, TARGET_K, CONF_THRESH, names);
    static std::vector<dl::detect::result_t> result;
    result.reserve(TARGET_K);
    static inference_process_args inference_args{
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
    constexpr size_t HTTP_BUFFER_SIZE = 1024;
    static uint8_t *buffer = static_cast<uint8_t *>(heap_caps_aligned_alloc(16, HTTP_BUFFER_SIZE,
                                                                            MALLOC_CAP_SPIRAM));
    static upload_process_args upload_args{
        upload_feed_back_queue,
        buffer,
        HTTP_BUFFER_SIZE};
    xTaskCreate(upload_process,
                "Upload",
                8192,
                &upload_args,
                1,
                nullptr);

    // 启动环境检测任务
    static std::atomic<uint16_t> report_env_delay;
    report_env_delay.store(1000);
    report_env_process_args report_env_args{report_env_delay};
    xTaskCreate(report_env_process,
                "ReporEnv",
                6144,
                &report_env_args,
                1,
                nullptr);

    init(panel.panel_handle(),
         panel.io_handle(),
         touch.touch_handle(),
         W, H);

    setup_ui(&guider_ui);
    lv_refr_now(lv_disp_get_default());
    ESP_LOGI(TAG, "UI ready");

    dropdown_capture_event_args dropdown_capture_args{
        capture_delay,
        running,
        capture_handle};
    lv_obj_add_event_cb(guider_ui.screen_2_Count_interval,
                        dropdown_capture_event_cb,
                        LV_EVENT_VALUE_CHANGED,
                        &dropdown_capture_args);
    lv_obj_add_event_cb(guider_ui.screen_2_Upload_interval,
                        dropdown_report_env_event_cb,
                        LV_EVENT_VALUE_CHANGED,
                        &report_env_delay);
    lv_obj_add_event_cb(guider_ui.screen_2_Temperature_slider,
                        slider_temp_event_cb,
                        LV_EVENT_VALUE_CHANGED,
                        nullptr);

    static dl::image::img_t screen_buffer{
        nullptr,
        320,
        320,
        dl::image::DL_IMAGE_PIX_TYPE_RGB565BE};
    screen_buffer.data = heap_caps_aligned_alloc(16,
                                                 screen_buffer.bytes(),
                                                 MALLOC_CAP_SPIRAM);
    static image_display_process_args image_display_args{
        result,
        cropped_buf,
        screen_buffer,
        flush_screen};
    xTaskCreate(image_display_process,
                "Image Display",
                5120,
                &image_display_args,
                1,
                nullptr);

    auto bus = touch.i2c_handle();
    xTaskCreate(env_display_process,
                "Env Display",
                5120,
                &bus,
                1,
                nullptr);

    while (true)
    {
        lv_lock();
        lv_timer_handler();
        lv_unlock();
        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

void touch_task(void *argv)
{
    while (true)
    {
        xSemaphoreTake(touch_sem, portMAX_DELAY); // 等待触摸中断
        // vTaskDelay(pdMS_TO_TICKS(10)); // 消抖+等待 FT6336 数据稳定
        lv_timer_handler(); // 处理触摸 + 刷新
        ESP_LOGI(TAG, "%d", uxTaskGetStackHighWaterMark(nullptr));
        vTaskDelay(pdMS_TO_TICKS(20));
        gpio_intr_enable(GPIO_CTP_INT);
    }
}

void mqtt_event_handler(void *argv,
                        esp_event_base_t base,
                        int32_t event_id,
                        void *event_data)
{
    const char *TAG = "MQTT event";
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
    // esp_mqtt_client_handle_t client = event->client;
    // int msg_id;

    auto args = static_cast<mqtt_event_handler_args *>(argv);

    switch (static_cast<esp_mqtt_event_id_t>(event_id))
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected");
        esp_mqtt_client_subscribe_single(MQTT::MQTT::instance().mqtt_handle(),
                                         std::format("server/{}/#", IOT::get_mac()).c_str(),
                                         1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "disconnected");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "subscribed, msg_id=%d, return code=0x%02x ", event->msg_id, (uint8_t)*event->data);
        // msg_id = esp_mqtt_client_publish(client, "topic/qos0", "data", 0, 0, 0);
        // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "unsubscribed, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "publish, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "receive data");
        // printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        {
            size_t len = event->topic_len;
            if (len >= 7 && memcmp(event->topic + len - 7, "/upload", 7) == 0)
            {
                char url[512] = {};
                int copy_len = (event->data_len < 512) ? event->data_len : 512;
                memcpy(url, event->data, copy_len);

                if (args->upload_feedback_queue)
                    xQueueSend(args->upload_feedback_queue, url, 0); // 0 = 不阻塞，满了就丢弃
            }
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "error");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            ESP_LOGE(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGE(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGE(TAG, "Last captured errno : %d (%s)", event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        }
        else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            ESP_LOGE(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        }
        else
        {
            ESP_LOGE(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}
