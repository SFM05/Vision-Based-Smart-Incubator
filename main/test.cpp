#include "cJSON.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_partition.h"
#include "freertos/projdefs.h"
#include "mqtt_client.h"
#include "sdkconfig.h"
#include "spi_flash_mmap.h"
#include <cstdio>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "iot_utils.hpp"
#include "mqtt.hpp"
#include "nvs_init.hpp"
#include "sdcard.hpp"
#include "wifi.hpp"

#include "test.hpp"

static const char *TAG = "test";

const char *ssid = CONFIG_IOT_WIFI_SSID;
const char *password = CONFIG_IOT_WIFI_PASSWORD;
const char *mqtt_server = CONFIG_IOT_MQTT_SERVER;
const char *mqtt_username = CONFIG_IOT_MQTT_USERNAME;
const char *mqtt_password = CONFIG_IOT_MQTT_PASSWORD;

void sdcard_test()
{
    esp_err_t err = sdcard::SDcard::instance().init(GPIO_NUM_43,  // CLK
                                                    GPIO_NUM_44,  // CMD(MOSI)
                                                    GPIO_NUM_39,  // D0(MISO)
                                                    GPIO_NUM_40,  // D1
                                                    GPIO_NUM_41,  // D2
                                                    GPIO_NUM_42); // D3(CS)
    if (err != ESP_OK) [[unlikely]]
    {
        ESP_LOGE(TAG, "SD card init failed.");
        return;
    }
    ESP_LOGI(TAG, "SD card init.");

    std::ifstream is("/sdcard/config.json");
    if (!is.is_open())
    {
        ESP_LOGE(TAG, "Cannot open file.");
        return;
    }
    std::stringstream buffer;
    buffer << is.rdbuf();
    std::cout << buffer.str() << '\n';
}

void wifi_example()
{
    NVS_init::nvs_init();
    ESP_LOGI(TAG, "NVS initailized.");

    IOT::Wifi::get_instance().init();
    ESP_LOGI(TAG, "Wifi initailized.");

    IOT::Wifi::get_instance().connect(ssid, password);
    ESP_LOGI(TAG, "Wifi connected.");

    // IOT::ntp_time_sync();

    MQTT::MQTT::get_instance().set_mqtt_cfg(mqtt_server,
                                            mqtt_username,
                                            mqtt_password);
    MQTT::MQTT::get_instance().mqtt_init();

    IOT::report_env(36.5, 50.0);

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

void wifi_test()
{
    NVS_init::nvs_init();
    ESP_LOGI(TAG, "NVS initailized.");

    IOT::Wifi::get_instance().init();
    ESP_LOGI(TAG, "Wifi initailized.");

    IOT::Wifi::get_instance().connect(ssid, password);
    ESP_LOGI(TAG, "Wifi connected.");

    IOT::ntp_time_sync();
}

void upload_image_test()
{
    NVS_init::nvs_init();
    ESP_LOGI(TAG, "NVS initailized.");

    IOT::Wifi::get_instance().init();
    ESP_LOGI(TAG, "Wifi initailized.");

    IOT::Wifi::get_instance().connect(ssid, password);
    ESP_LOGI(TAG, "Wifi connected.");

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

    const char *url = "";
    IOT::upload_file(url, img_ptr, IMAGE_SIZE);

    spi_flash_munmap(img_handle); // 释放映射
}

void mqtt_test()
{
    NVS_init::nvs_init();
    ESP_LOGI(TAG, "NVS initailized.");

    IOT::Wifi::get_instance().init();
    ESP_LOGI(TAG, "Wifi initailized.");

    IOT::Wifi::get_instance().connect(ssid, password);
    ESP_LOGI(TAG, "Wifi connected.");

    MQTT::MQTT::get_instance().set_mqtt_cfg(mqtt_server,
                                            mqtt_username,
                                            mqtt_password);
    MQTT::MQTT::get_instance().mqtt_init();

    esp_mqtt_client_publish(MQTT::MQTT::get_instance().mqtt_handle(),
                            "test/topic",
                            "This is a test message.",
                            0,
                            0,
                            0);
}
