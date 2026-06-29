#include "esp_err.h"
#include "esp_eth_driver.h"
#include "esp_eth_mac.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_netif_glue.h"
#include "esp_eth_phy.h"
#include "esp_eth_phy_ip101.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_types.h"

#include "eth.hpp"

static const char *TAG = "ethernet";

void got_ip_event_handler(void *arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Connected.");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
}

esp_err_t ethernet_init(esp_eth_mac_t **mac_p,
                        esp_eth_phy_t **phy_p,
                        esp_eth_netif_glue_handle_t *glue_handle_p,
                        esp_eth_handle_t *eth_handle_p,
                        esp_netif_t **eth_netif_p)
{
    // MAC 配置
    eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    // esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
    *mac_p = esp_eth_mac_new_esp32(&emac_config, &mac_config);
    if (*mac_p == NULL)
    {
        ESP_LOGE(TAG, "Create MAC instance failed.");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Create MAC instance sucess.");

    // PHY 配置
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.reset_gpio_num = 51; // 复位引脚GPIO51
    // esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
    *phy_p = esp_eth_phy_new_ip101(&phy_config);
    if (*phy_p == NULL)
    {
        ESP_LOGE(TAG, "Create PHY instance failed.");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Create PHY instance sucess.");

    // 创建以太网驱动实例
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(*mac_p, *phy_p);
    esp_err_t err = esp_eth_driver_install(&eth_config, eth_handle_p);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Init ethernet driver failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Init ethernet driver sucess.");

    // 初始化 TCP/IP 协议栈
    err = esp_netif_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Init netif failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Init netif sucess.");
    err = esp_event_loop_create_default();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Init eventloop failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Init eventloop sucess.");

    // 创建以太网网络接口
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    // esp_netif_t *eth_netif = esp_netif_new(&cfg);
    *eth_netif_p = esp_netif_new(&cfg);
    if (*eth_netif_p == NULL)
    {
        ESP_LOGE(TAG, "Create netif instance failed.");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Create netif instance sucess.");

    // 将以太网驱动连接至 TCP/IP 协议栈
    *glue_handle_p = esp_eth_new_netif_glue(*eth_handle_p);
    err = esp_netif_attach(*eth_netif_p, *glue_handle_p);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Attach netif failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Attach netif sucess.");

    // 注册 IP 事件处理程序
    err = esp_event_handler_register(IP_EVENT,
                                     IP_EVENT_ETH_GOT_IP,
                                     &got_ip_event_handler,
                                     NULL);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Register IP event failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Register IP event sucess.");

    // 启动以太网驱动
    err = esp_eth_start(*eth_handle_p);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Start ethernet failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Start ethernet sucess.");
    return ESP_OK;
}
