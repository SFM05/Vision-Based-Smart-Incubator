#include "camera.hpp"
#include "st7796.hpp"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_ldo_regulator.h"
#include "esp_heap_caps.h"
#include "esp_cam_ctlr_csi.h"
#include "esp_cam_ctlr.h"
#include "driver/isp.h"
#include "driver/isp_color.h"
#include "driver/isp_demosaic.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_cam_sensor_xclk.h"
#include "example_sensor_init.h"

static const char *TAG = "camera";

uint16_t cam_fb[CAM_DISP_W * CAM_DISP_H];
volatile bool cam_frame_ready = false;

#define CAM_SENSOR_HRES       800
#define CAM_SENSOR_VRES       640
#define CAM_SENSOR_FORMAT     "MIPI_2lane_24Minput_RAW8_800x640_50fps"
#define LDO_CHAN_ID           3
#define LDO_VOLTAGE_MV        2500
#define CSI_LANE_BITRATE_MBPS 200

static esp_cam_ctlr_handle_t s_cam_handle = NULL;
static isp_proc_handle_t s_isp_proc = NULL;
static esp_ldo_channel_handle_t s_ldo_chan = NULL;
static esp_cam_ctlr_trans_t s_cb_trans = {};

static bool IRAM_ATTR on_get_new_buf(esp_cam_ctlr_handle_t h, esp_cam_ctlr_trans_t *t, void *u)
{
    esp_cam_ctlr_trans_t *ut = (esp_cam_ctlr_trans_t *)u;
    t->buffer  = ut->buffer;
    t->buflen  = ut->buflen;
    return false;
}

static bool IRAM_ATTR on_trans_finished(esp_cam_ctlr_handle_t h, esp_cam_ctlr_trans_t *t, void *u)
{
    uint16_t *src = (uint16_t *)t->buffer;
    for (int y = 0; y < CAM_DISP_H; y++)
        for (int x = 0; x < CAM_DISP_W; x++) {
            uint16_t p = src[(y + (CAM_SENSOR_VRES - CAM_DISP_H) / 2) * CAM_SENSOR_HRES + x + (CAM_SENSOR_HRES - CAM_DISP_W) / 2];
            cam_fb[y * CAM_DISP_W + x] = (p >> 8) | (p << 8);
        }
    cam_frame_ready = true;
    return false;
}

esp_err_t camera_init(void)
{
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = LDO_CHAN_ID,
        .voltage_mv = LDO_VOLTAGE_MV,
    };
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &s_ldo_chan), TAG, "ldo acquire fail");

    gpio_set_direction((gpio_num_t)PIN_CAM_RST, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)PIN_CAM_RST, 0);
    gpio_set_direction((gpio_num_t)PIN_CAM_PWDN, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)PIN_CAM_PWDN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level((gpio_num_t)PIN_CAM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_cam_sensor_xclk_handle_t xclk = NULL;
    ESP_RETURN_ON_ERROR(
        esp_cam_sensor_xclk_allocate(ESP_CAM_SENSOR_XCLK_ESP_CLOCK_ROUTER, &xclk),
        TAG, "xclk alloc fail");
    esp_cam_sensor_xclk_config_t xclk_cfg = {
        .esp_clock_router_cfg = {
            .xclk_pin = (gpio_num_t)PIN_CAM_MCLK,
            .xclk_freq_hz = 24 * 1000 * 1000,
        },
    };
    ESP_RETURN_ON_ERROR(
        esp_cam_sensor_xclk_start(xclk, &xclk_cfg), TAG, "xclk start fail");

    example_sensor_config_t sensor_cfg = {
        .i2c_port_num   = I2C_NUM_0,
        .i2c_sda_io_num = PIN_CAM_SDA,
        .i2c_scl_io_num = PIN_CAM_SCL,
        .port           = ESP_CAM_SENSOR_MIPI_CSI,
        .format_name    = CAM_SENSOR_FORMAT,
    };
    example_sensor_handle_t sensor_handle;
    example_sensor_init(&sensor_cfg, &sensor_handle);

    size_t fb_sz = CAM_SENSOR_HRES * CAM_SENSOR_VRES * 2;

    esp_cam_ctlr_csi_config_t csi_cfg = {
        .ctlr_id                = 0,
        .clk_src                = MIPI_CSI_PHY_CLK_SRC_DEFAULT,
        .h_res                  = CAM_SENSOR_HRES,
        .v_res                  = CAM_SENSOR_VRES,
        .data_lane_num          = 2,
        .lane_bit_rate_mbps     = CSI_LANE_BITRATE_MBPS,
        .input_data_color_type  = CAM_CTLR_COLOR_RAW8,
        .output_data_color_type = CAM_CTLR_COLOR_RGB565,
        .queue_items            = 1,
        .byte_swap_en           = false,
        .bk_buffer_dis          = false,
    };
    ESP_RETURN_ON_ERROR(esp_cam_new_csi_ctlr(&csi_cfg, &s_cam_handle), TAG, "csi new");

    void *isp_buf = esp_cam_ctlr_alloc_buffer(s_cam_handle, fb_sz, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
    if (!isp_buf) return ESP_ERR_NO_MEM;

    s_cb_trans = { .buffer = isp_buf, .buflen = fb_sz };

    esp_cam_ctlr_evt_cbs_t cbs = {
        .on_get_new_trans   = on_get_new_buf,
        .on_trans_finished  = on_trans_finished,
    };
    ESP_ERROR_CHECK(esp_cam_ctlr_register_event_callbacks(s_cam_handle, &cbs, &s_cb_trans));
    ESP_ERROR_CHECK(esp_cam_ctlr_enable(s_cam_handle));

    isp_proc_handle_t isp = NULL;
    esp_isp_processor_cfg_t isp_cfg = {
        .clk_src                = ISP_CLK_SRC_DEFAULT,
        .clk_hz                 = 80 * 1000 * 1000,
        .input_data_source      = ISP_INPUT_DATA_SOURCE_CSI,
        .input_data_color_type  = ISP_COLOR_RAW8,
        .output_data_color_type = ISP_COLOR_RGB565,
        .yuv_range              = ISP_COLOR_RANGE_FULL,
        .yuv_std                = ISP_YUV_CONV_STD_BT709,
        .has_line_start_packet  = false,
        .has_line_end_packet    = false,
        .h_res                  = CAM_SENSOR_HRES,
        .v_res                  = CAM_SENSOR_VRES,
        .bayer_order            = COLOR_RAW_ELEMENT_ORDER_GBRG,
    };
    ESP_ERROR_CHECK(esp_isp_new_processor(&isp_cfg, &isp));
    ESP_ERROR_CHECK(esp_isp_demosaic_configure(isp, NULL));
    ESP_ERROR_CHECK(esp_isp_demosaic_enable(isp));
    esp_isp_color_config_t color_cfg = {
        .color_contrast = 127,
        .color_saturation = 127,
        .color_hue = 0,
        .color_brightness = 10,
        .flags = { .update_once_configured = 1 },
    };
    ESP_ERROR_CHECK(esp_isp_color_configure(isp, &color_cfg));
    ESP_ERROR_CHECK(esp_isp_color_enable(isp));

    ESP_ERROR_CHECK(esp_isp_enable(isp));
    s_isp_proc = isp;

    ESP_ERROR_CHECK(esp_cam_ctlr_start(s_cam_handle));

    ESP_LOGI(TAG, "camera initialized %dx%d", CAM_SENSOR_HRES, CAM_SENSOR_VRES);
    return ESP_OK;
}
