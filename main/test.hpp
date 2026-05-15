#include "esp_log.h"
#include "sdkconfig.h"
#include "wifi.hpp"
#include <string>

const char *ssid = CONFIG_ESP_WIFI_SSID;
const char *password = CONFIG_ESP_WIFI_PASSWORD;

void wifi_test()
{
    ESP_LOGI("main", "Get into wifi_test:");
    IOT::Wifi::get_instance().init_nvs();
    ESP_LOGI("test", "NVS initailized.");
    IOT::Wifi::get_instance().init();
    ESP_LOGI("test", "Wifi initailized.");
    IOT::Wifi::get_instance().connect(ssid, password);
    ESP_LOGI("test", "Wifi connected.");
}