#include "esp_eth_mac_esp.h"

#ifdef __cplusplus
extern "C"
{
    eth_esp32_emac_config_t eth_esp32_emac_default_config();
}
#else
eth_esp32_emac_config_t eth_esp32_emac_default_config();
#endif