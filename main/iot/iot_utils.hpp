#pragma once

#include "esp_err.h"
#include "esp_mac.h"
#include <cstdint>
#include <format>
#include <string>

namespace IOT
{
    esp_err_t ntp_time_sync();

    esp_err_t upload_file(const char *url,  // 预签名 URL（含 query 参数）
                          const void *data, // JPEG 数据指针
                          size_t len);      // 数据长度

    esp_err_t upload_file(const char *url,
                          const char *path,
                          uint8_t *buffer,
                          size_t size);

    esp_err_t report_env(float temp, float hum, uint16_t co2);

    inline std::string get_mac()
    {
        uint8_t mac[6];
        esp_efuse_mac_get_default(mac);
        return std::format("{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}",
                           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}
