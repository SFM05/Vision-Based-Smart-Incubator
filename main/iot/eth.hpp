#pragma once

#include "esp_bit_defs.h"
#include "esp_err.h"
#include "esp_eth_netif_glue.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <cstdint>

namespace ETH
{
    class Ethernet
    {
    public:
        static Ethernet &instance()
        {
            static Ethernet m_instance;
            return m_instance;
        }
        esp_err_t init();
        void deinit();
        bool status() { return m_status; }

    private:
        Ethernet() : m_status{false}, m_eth_event_group{xEventGroupCreate()} {}
        Ethernet(const Ethernet &) = delete;
        ~Ethernet() = default;
        Ethernet &operator=(const Ethernet &) = delete;

        static void got_ip_event_handler(void *arg,
                                         esp_event_base_t event_base,
                                         int32_t event_id,
                                         void *event_data);

        esp_eth_mac_t *m_mac;
        esp_eth_phy_t *m_phy;
        esp_eth_netif_glue_handle_t m_glue_handle;
        esp_eth_handle_t m_eth_handle;
        esp_netif_t *m_eth_netif;
        bool m_status;

        EventGroupHandle_t m_eth_event_group;
        static constexpr EventBits_t NO_IP = BIT0;
        static constexpr EventBits_t GET_IP = BIT1;
    };
}
