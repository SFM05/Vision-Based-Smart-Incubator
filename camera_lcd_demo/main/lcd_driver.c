/**
 * @file lcd_driver.c
 * @brief LCD 显示驱动模块实现 - EK79007 MIPI DSI 面板初始化
 *
 * 硬件配置:
 *   - 面板: EK79007, 1024x600 像素, RGB565 (16bpp)
 *   - 接口: MIPI DSI (2 lane), DBI 寄存器命令通道
 *   - 复位: GPIO27 (低有效)
 *   - 背光: GPIO26 (高电平开启)
 *   - PHY 电源: LDO channel 3 @ 2.5V
 */

#include "lcd_driver.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_ek79007.h"
#include "soc/soc_caps.h"
#include "esp_ldo_regulator.h"

static const char *TAG = "lcd_driver";

/* ---------- 硬件引脚与参数 ---------- */

#define LCD_RST_GPIO                GPIO_NUM_27    ///< 硬件复位引脚
#define LCD_BL_GPIO                 GPIO_NUM_26    ///< 背光控制引脚

#define LCD_WIDTH                   1024           ///< 面板水平分辨率
#define LCD_HEIGHT                  600            ///< 面板垂直分辨率
#define LCD_BPP                     16             ///< 每像素位数（RGB565）

#define MIPI_DSI_PHY_PWR_LDO_CHAN           (3)    ///< MIPI DSI PHY LDO 通道号
#define MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV     (2500)  ///< LDO 输出电压 (mV)

/** MIPI DSI PHY LDO 通道句柄（模块内持有，deinit 时释放） */
static esp_ldo_channel_handle_t ldo_mipi_phy = NULL;

/* ========================= 公共接口实现 ========================= */

esp_err_t lcd_ek79007_init(esp_lcd_panel_handle_t *panel_handle)
{
#if !SOC_MIPI_DSI_SUPPORTED
    ESP_LOGE(TAG, "MIPI DSI not supported on this chip");
    return ESP_ERR_NOT_SUPPORTED;
#endif

    ESP_LOGI(TAG, "Initializing EK79007 LCD (MIPI DSI)");

    /* ---- Step 1: 配置背光 GPIO，初始状态关闭 ---- */
    gpio_config_t bl_gpio_conf = {
        .pin_bit_mask = (1ULL << LCD_BL_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&bl_gpio_conf);
    gpio_set_level(LCD_BL_GPIO, 0);  /* 先关闭背光，避免初始化过程闪屏 */

    /* ---- Step 2: 开启 MIPI DSI PHY 电源 ----
     *
     * ESP32-P4 的 MIPI DSI PHY 需要从 "No Power" 状态进入 "Shutdown" 状态，
     * 必须通过 LDO regulator 提供电源才能正常工作。
     */
    ESP_LOGI(TAG, "Power on MIPI DSI PHY LDO");
    esp_ldo_channel_config_t ldo_config = {
        .chan_id      = MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv   = MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    esp_err_t ret = esp_ldo_acquire_channel(&ldo_config, &ldo_mipi_phy);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to acquire LDO channel: %s", esp_err_to_name(ret));
        return ret;
    }

    /* ---- Step 3: 创建 MIPI DSI 总线 ---- */
    ESP_LOGI(TAG, "Initialize MIPI DSI bus");
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_config = EK79007_PANEL_BUS_DSI_2CH_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    /* ---- Step 4: 安装面板 IO (DBI 接口用于寄存器命令传输) ---- */
    ESP_LOGI(TAG, "Install panel IO (DBI for register commands)");
    esp_lcd_panel_io_handle_t mipi_dbi_io = NULL;
    esp_lcd_dbi_io_config_t dbi_config = EK79007_PANEL_IO_DBI_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));

    /* ---- Step 5: 配置 DPI 面板参数（视频数据通道） ----
     *
     * 使用 RGB565 像素格式匹配 ISP 输出，
     * 分辨率/时序使用 EK79007 官方预设值。
     */
    ESP_LOGI(TAG, "Configure DPI panel (RGB565 %dx%d@60Hz)", LCD_WIDTH, LCD_HEIGHT);
    const esp_lcd_dpi_panel_config_t dpi_config =
        EK79007_1024_600_PANEL_60HZ_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);

    /* ---- Step 6: 创建并安装 EK79007 面板驱动 ---- */
    ESP_LOGI(TAG, "Install EK79007 panel driver");
    ek79007_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus    = mipi_dsi_bus,
            .dpi_config = &dpi_config,
            .lane_num   = 2,              /* 2-lane DSI */
        },
    };

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST_GPIO,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BPP,
        .vendor_config  = &vendor_config,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_ek79007(mipi_dbi_io, &panel_config, panel_handle));

    /* ---- Step 7: 复位、初始化、开启背光 ---- */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(*panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(*panel_handle));  /* 内含显示开启命令 */

    gpio_set_level(LCD_BL_GPIO, 1);  /* 开启背光 */

    ESP_LOGI(TAG, "LCD initialization complete (%dx%d RGB565)", LCD_WIDTH, LCD_HEIGHT);
    return ESP_OK;
}

esp_err_t lcd_ek79007_deinit(esp_lcd_panel_handle_t panel_handle)
{
    if (panel_handle != NULL) {
        esp_lcd_panel_del(panel_handle);
        ESP_LOGI(TAG, "LCD deinitialized");
    }
    /* 释放 MIPI DSI PHY LDO 电源通道 */
    if (ldo_mipi_phy != NULL) {
        esp_ldo_release_channel(ldo_mipi_phy);
        ldo_mipi_phy = NULL;
        ESP_LOGI(TAG, "MIPI DSI PHY LDO released");
    }
    return ESP_OK;
}
