#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif_ip_addr.h"
#include <format>
#include <string>

#include "eth.hpp"

namespace ETH
{
    esp_err_t Ethernet::init()
    {
        esp_err_t err = ethernet_init(&m_mac,
                                      &m_phy,
                                      &m_glue_handle,
                                      &m_eth_handle,
                                      &m_eth_netif);
        if (err != ESP_OK) [[unlikely]]
        {
            m_status = false;
            return err;
        }
        m_status = true;
        return ESP_OK;
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