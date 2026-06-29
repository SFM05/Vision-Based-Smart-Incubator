#include "cJSON.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "sdkconfig.h"
#include "spi_flash_mmap.h"
#include <cstdint>
#include <cstring>
#include <format>
#include <iostream>
#include <string>

#include "iot_utils.hpp"
#include "mqtt.hpp"
#include "nvs_init.hpp"
#include "wifi.hpp"

#include "yolo26.hpp"
#include "yolo_inference.hpp"

#include "ft6336_driver.hpp"
#include "st7796_driver.hpp"

#include "eth.hpp"
#include "ping/ping_sock.h"

#include "test.hpp"

static const char *TAG = "test";

const char *ssid = CONFIG_IOT_WIFI_SSID;
const char *password = CONFIG_IOT_WIFI_PASSWORD;
const char *mqtt_server = CONFIG_IOT_MQTT_SERVER;
const char *mqtt_username = CONFIG_IOT_MQTT_USERNAME;
const char *mqtt_password = CONFIG_IOT_MQTT_PASSWORD;

// LCD参数
constexpr uint16_t HORIZONRAL_RES = 480;
constexpr uint16_t VERTICAL_RES = 320;

constexpr gpio_num_t GPIO_CS = GPIO_NUM_22;
constexpr gpio_num_t GPIO_RST = GPIO_NUM_23;
constexpr gpio_num_t GPIO_RS = GPIO_NUM_26; // LCD_RS (D/C)
constexpr gpio_num_t GPIO_MOSI = GPIO_NUM_27;
constexpr gpio_num_t GPIO_CLK = GPIO_NUM_32;

constexpr gpio_num_t GPIO_CTP_SCL = GPIO_NUM_50;
constexpr gpio_num_t GPIO_CTP_RST = GPIO_NUM_49;
constexpr gpio_num_t GPIO_CTP_SDA = GPIO_NUM_5;
constexpr gpio_num_t GPIO_CTP_INT = GPIO_NUM_4;

constexpr unsigned int CLK_FREQUENCY = 40e6; // 最大80MHz

// IoT functions
void iot_test()
{
    ETH::Ethernet::instance().init();
    ESP_LOGI(TAG, "Ethernet connected.");
    vTaskDelay(1000);

    // IOT::ntp_time_sync();

    MQTT::MQTT::get_instance().set_mqtt_cfg(mqtt_server,
                                            mqtt_username,
                                            mqtt_password);
    MQTT::MQTT::get_instance().mqtt_init();

    IOT::report_env(36.5, 50.0, 0);

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

    time_t now = time(NULL); // 秒级时间戳
    struct tm tm_info;
    localtime_r(&now, &tm_info); // 转本地时间
    char time[32];
    strftime(time, sizeof(time), "%Y-%m-%d %H:%M:%S", &tm_info);
    // NTP同步失败，时间位1970。上报空时间戳，使用服务器时间
    if (time[0] != '2')
        memset(time, 0, sizeof(time));

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "timestamp", time);
    cJSON_AddNumberToObject(root, "plateid", 0);
    cJSON_AddStringToObject(root, "imgpath", "test_img");
    cJSON_AddStringToObject(root, "txtpath", "text_txt");
    cJSON_AddNumberToObject(root, "number", -1);
    char *json_str = cJSON_Print(root);

    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    esp_mqtt_client_publish(MQTT::MQTT::get_instance().mqtt_handle(),
                            std::format("device/{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}/upload", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]).c_str(),
                            json_str,
                            0,
                            1,
                            0);
    cJSON_Delete(root);

    char msg[512] = {};
    while (true)
    {
        // 阻塞等待队列消息
        if (xQueueReceive(MQTT::MQTT::get_instance().m_upload_queue,
                          msg,
                          pdMS_TO_TICKS(30000)) == pdTRUE)
        {
            ESP_LOGI(TAG, "Message: %s", msg);

            root = cJSON_Parse(msg);
            if (root == nullptr)
            {
                ESP_LOGE(TAG, "Parse JSON error.");
                return;
            }
            char url[512];
            char path[64];
            char time[32];
            strncpy(url, cJSON_GetObjectItem(root, "url")->valuestring, 512);
            strncpy(path, cJSON_GetObjectItem(root, "path")->valuestring, 54);
            strncpy(time, cJSON_GetObjectItem(root, "timestamp")->valuestring, 32);
            cJSON_Delete(root);
            ESP_LOGI(TAG, "%s", url);

            if (strcmp(path, "test_img") == 0)
            {
                IOT::upload_file(url, img_ptr, IMAGE_SIZE);

                root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, "timestamp", time);
                cJSON_AddNumberToObject(root, "plateid", 0);
                json_str = cJSON_Print(root);
                uint8_t mac[6];
                esp_efuse_mac_get_default(mac);
                esp_mqtt_client_publish(MQTT::MQTT::get_instance().mqtt_handle(),
                                        std::format("device/{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}/uploadsuccess", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]).c_str(),
                                        json_str,
                                        0,
                                        1,
                                        0);
                // 需要在url和path使用完之后释放
                cJSON_Delete(root);
            }
        }
        else
        {
            ESP_LOGW(TAG, "Unable to receive feedback or stream has finished.");
            break;
        }
    }

    spi_flash_munmap(img_handle); // 释放映射
}

void yolo_test()
{
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
    // auto img_cropped = crop(img, {left, top, right, bottom});
    // auto img_cropped = crop(img, 0, 0, 640, 640);
    auto img_cropped = img;
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

    std::cout << std::format("Detected {} colonies\n", results.size()).c_str();
    for (const auto &item : results)
    {
        std::cout << std::format("{:>5}{:>5}{:>5}{:>5}{:>8.4f}\n", item.box[0], item.box[1], item.box[2], item.box[3], item.score);
    }

    delete model;
}

using touch_task_param_t = std::pair<esp_lcd_panel_handle_t, esp_lcd_touch_handle_t>;

static SemaphoreHandle_t touch_sem; // 触摸二值信号量

// 触摸中断回调 — 在 ISR 中运行，必须 IRAM_ATTR，做最少的事
static void IRAM_ATTR my_touch_isr_cb(esp_lcd_touch_handle_t touch_handle)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(touch_sem, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

// 触摸处理任务
static void touch_task(void *argv)
{
    touch_task_param_t *args = static_cast<touch_task_param_t *>(argv);
    auto panel_handle = args->first;
    auto touch_handle = args->second;

    uint16_t *buffer_white = (uint16_t *)heap_caps_malloc(50 * 50 * sizeof(uint16_t), MALLOC_CAP_DMA);
    uint16_t *buffer_black = (uint16_t *)heap_caps_malloc(50 * 50 * sizeof(uint16_t), MALLOC_CAP_DMA);
    memset(buffer_white, 0xFFFF, 50 * 50 * sizeof(uint16_t));
    memset(buffer_black, 0, 50 * 50 * sizeof(uint16_t));

    uint16_t x = HORIZONRAL_RES / 2;
    uint16_t y = VERTICAL_RES / 2;
    uint16_t strength;
    while (true)
    {
        xSemaphoreTake(touch_sem, portMAX_DELAY); // 等中断
        esp_lcd_touch_read_data(touch_handle);    // I2C 读取触摸数据

        esp_lcd_touch_point_data_t points[1] = {0};
        uint8_t touch_cnt = 0;
        esp_lcd_touch_get_data(touch_handle, points, &touch_cnt, 1);
        if (touch_cnt > 0)
        {
            // 先清空原来的区域
            esp_lcd_panel_draw_bitmap(panel_handle,
                                      std::max(x - 25, 0),
                                      std::max(y - 25, 0),
                                      std::min(x + 25, static_cast<int>(VERTICAL_RES)),
                                      std::min(y + 25, static_cast<int>(HORIZONRAL_RES)),
                                      buffer_white);

            x = points[0].x;
            y = points[0].y;
            strength = points[0].strength;
            std::string msg = std::format("Touch: x={}, y={}, strength={}", x, y, strength);
            ESP_LOGI(TAG, "%s", msg.c_str());
            // 绘制新区域
            esp_lcd_panel_draw_bitmap(panel_handle,
                                      std::max(x - 25, 0),
                                      std::max(y - 25, 0),
                                      std::min(x + 25, static_cast<int>(VERTICAL_RES)),
                                      std::min(y + 25, static_cast<int>(HORIZONRAL_RES)),
                                      buffer_black);
        }
        else
        {
            ESP_LOGI(TAG, "No touch");
            esp_lcd_panel_draw_bitmap(panel_handle, x - 25, y - 25, x + 25, y + 25, buffer_white);
        }
    }
}

void lcd_test()
{

    // 初始化屏幕
    LCD::ST7796 panel_driver{SPI2_HOST, HORIZONRAL_RES, VERTICAL_RES};

    panel_driver.set_spi_cfg(GPIO_MOSI, GPIO_CLK);
    panel_driver.set_io_cfg(GPIO_CS, GPIO_RS, CLK_FREQUENCY);
    panel_driver.set_panel_cfg(GPIO_RST);

    esp_err_t err;
    err = panel_driver.spi_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "LCD initialized");

    err = panel_driver.io_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize LCD IO: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "LCD IO initialized");

    err = panel_driver.panel_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize LCD panel: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "LCD panel initialized");

    uint16_t *buffer = (uint16_t *)heap_caps_malloc(HORIZONRAL_RES * VERTICAL_RES * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    memset(buffer, 0xFF, HORIZONRAL_RES * VERTICAL_RES * sizeof(uint16_t));
    esp_lcd_panel_draw_bitmap(panel_driver.panel_handle(), 0, 0, VERTICAL_RES, HORIZONRAL_RES, buffer);
    heap_caps_free(buffer);

    // 初始化触控
    touch_sem = xSemaphoreCreateBinary();

    LCD::FT6336 touch_driver{HORIZONRAL_RES, VERTICAL_RES};

    touch_driver.set_i2c_config(GPIO_CTP_SDA, GPIO_CTP_SCL);
    touch_driver.set_touch_config(GPIO_CTP_RST, GPIO_CTP_INT, my_touch_isr_cb);

    err = touch_driver.i2c_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "I2C initialized");

    err = touch_driver.io_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize LCD IO: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Touch IO initialized");

    err = touch_driver.touch_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize touch: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Touch initialized");

    touch_task_param_t argv = std::make_pair(panel_driver.panel_handle(),
                                             touch_driver.touch_handle());

    xTaskCreate(touch_task, "touch", 4096, static_cast<void *>(&argv), 5, NULL);
}
