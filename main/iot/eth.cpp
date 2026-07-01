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
#include "freertos/idf_additions.h"
#include <cstdint>

#include "eth.hpp"
#include "private/utils.h"

static const char *TAG = "ethernet";

namespace ETH
{
    constexpr unsigned int TIMEOUT = 2000;

    void Ethernet::got_ip_event_handler(void *arg,
                                        esp_event_base_t event_base,
                                        int32_t event_id,
                                        void *event_data)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        const esp_netif_ip_info_t *ip_info = &event->ip_info;

        ESP_LOGI(TAG, "   ETHIP:" IPSTR, IP2STR(&ip_info->ip));
        ESP_LOGI(TAG, " ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
        ESP_LOGI(TAG, "   ETHGW:" IPSTR, IP2STR(&ip_info->gw));

        xEventGroupSetBits(Ethernet::instance().m_eth_event_group,
                           Ethernet::GET_IP);
    }

    esp_err_t Ethernet::init()
    {
        // MAC 配置
        eth_esp32_emac_config_t emac_config = eth_esp32_emac_default_config();
        eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
        // esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
        m_mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
        if (m_mac == NULL) [[unlikely]]
        {
            ESP_LOGE(TAG, "Create MAC instance failed.");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Create MAC instance sucess.");

        // PHY 配置
        eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
        phy_config.reset_gpio_num = 51; // 复位引脚GPIO51
        // esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
        m_phy = esp_eth_phy_new_ip101(&phy_config);
        if (m_phy == NULL) [[unlikely]]
        {
            ESP_LOGE(TAG, "Create PHY instance failed.");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Create PHY instance sucess.");

        // 创建以太网驱动实例
        esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(m_mac, m_phy);
        esp_err_t err = esp_eth_driver_install(&eth_config, &m_eth_handle);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Init ethernet driver failed: %s", esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "Init ethernet driver sucess.");

        // 初始化 TCP/IP 协议栈
        err = esp_netif_init();
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Init netif failed: %s", esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "Init netif sucess.");
        err = esp_event_loop_create_default();
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Init eventloop failed: %s", esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "Init eventloop sucess.");

        // 创建以太网网络接口
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        // esp_netif_t *eth_netif = esp_netif_new(&cfg);
        m_eth_netif = esp_netif_new(&cfg);
        if (m_eth_netif == NULL) [[unlikely]]
        {
            ESP_LOGE(TAG, "Create netif instance failed.");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Create netif instance sucess.");

        // 将以太网驱动连接至 TCP/IP 协议栈
        m_glue_handle = esp_eth_new_netif_glue(m_eth_handle);
        err = esp_netif_attach(m_eth_netif, m_glue_handle);
        if (err != ESP_OK) [[unlikely]]
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
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Register IP event failed: %s", esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "Register IP event sucess.");

        // 启动以太网驱动
        err = esp_eth_start(m_eth_handle);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Start ethernet failed: %s", esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "Start ethernet sucess.");

        // 等待获取到网关ip
        EventBits_t bits = xEventGroupWaitBits(m_eth_event_group,
                                               Ethernet::GET_IP,
                                               pdFALSE,
                                               pdFALSE,
                                               pdMS_TO_TICKS(TIMEOUT));
        if (bits & GET_IP) [[likely]]
        {
            ESP_LOGI(TAG, "Ethernet connected success");
            m_status = true;
            return ESP_OK;
        }
        else
        {
            ESP_LOGE(TAG, "Ethernet connection timeout after %d ms", TIMEOUT);
            m_status = false;
            return ESP_FAIL;
        }
    }

    void Ethernet::deinit()
    {
        m_status = false;
        // 1. 停止以太网驱动
        esp_eth_stop(m_eth_handle);

        // 2. 删除 netif glue（解除驱动与TCP/IP栈的绑定）
        esp_eth_del_netif_glue(m_glue_handle);

        // 3. 销毁网络接口
        esp_netif_destroy(m_eth_netif);

        // 4. 卸载以太网驱动
        esp_eth_driver_uninstall(m_eth_handle);

        // 5. 删除 MAC 和 PHY 实例
        m_mac->del(m_mac);
        m_phy->del(m_phy);

        // 6. 注销事件处理程序
        esp_event_handler_unregister(IP_EVENT,
                                     IP_EVENT_ETH_GOT_IP,
                                     &got_ip_event_handler);
        // ESP_ERROR_CHECK(esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler));

        // 7. 删除事件循环
        esp_event_loop_delete_default();
    }
}