#include "esp_err.h"
#include "esp_eth_netif_glue.h"
#include "esp_netif.h"
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif
    esp_err_t ethernet_init(esp_eth_mac_t **mac_p,
                            esp_eth_phy_t **phy_p,
                            esp_eth_netif_glue_handle_t *glue_handle_p,
                            esp_eth_handle_t *eth_handle_p,
                            esp_netif_t **eth_netif_p);
    void got_ip_event_handler(void *arg,
                              esp_event_base_t event_base,
                              int32_t event_id,
                              void *event_data);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
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
        Ethernet() : m_status{false} {};
        Ethernet(const Ethernet &) = delete;
        ~Ethernet() = default;
        Ethernet &operator=(const Ethernet &) = delete;

        esp_eth_mac_t *m_mac;
        esp_eth_phy_t *m_phy;
        esp_eth_netif_glue_handle_t m_glue_handle;
        esp_eth_handle_t m_eth_handle;
        esp_netif_t *m_eth_netif;
        bool m_status;
    };
}
#endif
