#include "sdkconfig.h"

#if CONFIG_SD_CARD_SOIO_MOD
#include "driver/sdmmc_default_configs.h"
#include "driver/sdmmc_host.h"
#endif
#if CONFIG_SD_CARD_SPI_MOD
#include "driver/sdspi_host.h"
#include "esp_ldo_regulator.h"
#include "soc/gpio_num.h"
#endif
#include "esp_err.h"
#include "esp_vfs_fat.h"

#include "sdcard.hpp"

static const char *TAG = "SD card";

namespace sdcard
{
    esp_err_t SDcard::init()
    {
        esp_err_t err;

        esp_vfs_fat_sdmmc_mount_config_t mount_config;
        mount_config.format_if_mount_failed = false; // 挂载失败时格式化
        mount_config.max_files = 5;
        mount_config.allocation_unit_size = 16 * 1024;

#if CONFIG_SD_CARD_SOIO_MOD
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.slot = SDMMC_HOST_SLOT_0;
        host.unaligned_multi_block_rw_max_chunk_size = 8;
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        // slot_config.width = 4; // 4 线模式；设为 1 则用 1 线模式

        err = esp_vfs_fat_sdmmc_mount("/sdcard",
                                      &host,
                                      &slot_config,
                                      &mount_config,
                                      &m_card);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Failed to initialize SD: %s", esp_err_to_name(err));
            return err;
        }
#endif

#if CONFIG_SD_CARD_SPI_MOD
        esp_ldo_channel_config_t ldo_cfg{};
        ldo_cfg.chan_id = 4;
        ldo_cfg.voltage_mv = 3300;
        ldo_cfg.flags.adjustable = false;
        err = esp_ldo_acquire_channel(&ldo_cfg, &m_ldo_chan_handle);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Fail to adjust voltage of LDO_VO4.");
            esp_ldo_release_channel(m_ldo_chan_handle);
            return err;
        }
        ESP_LOGI(TAG, "Adjust voltage of LDO_VO4 to 3.3V.");

        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.unaligned_multi_block_rw_max_chunk_size = 2;
        spi_bus_config_t bus_cfg{};
        bus_cfg.mosi_io_num = GPIO_NUM_44; // CMD
        bus_cfg.miso_io_num = GPIO_NUM_39; // D0
        bus_cfg.sclk_io_num = GPIO_NUM_43; // clk
        bus_cfg.quadwp_io_num = GPIO_NUM_NC;
        bus_cfg.quadhd_io_num = GPIO_NUM_NC;
        bus_cfg.max_transfer_sz = 4000;
        err = spi_bus_initialize(SDSPI_DEFAULT_HOST,
                                 &bus_cfg,
                                 SDSPI_DEFAULT_DMA);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(err));
            return err;
        }

        sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_config.gpio_cs = GPIO_NUM_42; // D3
        slot_config.host_id = SDSPI_DEFAULT_HOST;
        err = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &m_card);
        if (err != ESP_OK) [[unlikely]]
        {
            ESP_LOGE(TAG, "Failed to initialize SD: %s", esp_err_to_name(err));
            return err;
        }
#endif
        m_status = true;
        return ESP_OK;
    }

    void SDcard::deinit()
    {
        esp_vfs_fat_sdcard_unmount("/sdcard", m_card);
#if CONFIG_SD_CARD_SPI_MOD
        esp_ldo_release_channel(m_ldo_chan_handle);
#endif
        m_status = false;
    }
}