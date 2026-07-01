#include "utils.h"
#include "esp_eth_mac_esp.h"
eth_esp32_emac_config_t eth_esp32_emac_default_config()
{
    return ((eth_esp32_emac_config_t)ETH_ESP32_EMAC_DEFAULT_CONFIG());
}