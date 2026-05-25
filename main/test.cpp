#include "esp_err.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "sdkconfig.h"
#include "spi_flash_mmap.h"
#include <string>

#include "iot_utils.hpp"
#include "mqtt.hpp"
#include "nvs_init.hpp"
#include "wifi.hpp"

#include "test.hpp"

static const char *TAG = "test";

const char *ssid = CONFIG_WIFI_SSID;
const char *password = CONFIG_WIFI_PASSWORD;

void wifi_example()
{
    NVS_init::nvs_init();
    ESP_LOGI(TAG, "NVS initailized.");

    IOT::Wifi::get_instance().init();
    ESP_LOGI(TAG, "Wifi initailized.");

    IOT::Wifi::get_instance().connect(ssid, password);
    ESP_LOGI(TAG, "Wifi connected.");

    // IOT::ntp_time_sync();

    MQTT::MQTT::get_instance().set_mqtt_cfg(CONFIG_MQTT_SERVER,
                                            CONFIG_MQTT_USERNAME,
                                            CONFIG_MQTT_PASSWORD);
    MQTT::MQTT::get_instance().mqtt_init();

    IOT::report_env(36.5, 50.0);
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

    MQTT::MQTT::get_instance().set_mqtt_cfg(CONFIG_MQTT_SERVER,
                                            CONFIG_MQTT_USERNAME,
                                            CONFIG_MQTT_PASSWORD);
    MQTT::MQTT::get_instance().mqtt_init();

    esp_mqtt_client_publish(MQTT::MQTT::get_instance().mqtt_handle(),
                            "test/topic",
                            "This is a test message.",
                            0,
                            0,
                            0);
}
