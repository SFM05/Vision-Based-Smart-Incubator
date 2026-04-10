/**
 * @file camera_driver.c
 * @brief 摄像头驱动模块实现 - OV5647 传感器初始化
 *
 * 硬件配置:
 *   - 传感器: OV5647 (5MP, MIPI CSI 接口)
 *   - I2C:    GPIO7 (SDA) + GPIO8 (SCL), 端口 0, 400kHz
 *   - 输出:   V4L2 设备节点 /dev/video0, ISP 处理为 RGB565
 */

#include "camera_driver.h"
#include "esp_log.h"
#include "esp_video_init.h"
#include "esp_video_device.h"

static const char *TAG = "camera_driver";

/* ---------- I2C/SCCB 硬件配置 ---------- */

#define CAM_I2C_PORT        0       ///< I2C 端口号
#define CAM_I2C_SCL_GPIO    8       ///< SCL 引脚
#define CAM_I2C_SDA_GPIO    7       ///< SDA 引脚

/* ========================= 公共接口实现 ========================= */

esp_err_t camera_ov5647_init(camera_driver_ctx_t *ctx)
{
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing Camera via esp_video_init (MIPI CSI + V4L2)");

    /*
     * 配置 MIPI CSI 摄像头（OV5647）
     *
     * esp_video_init() 内部自动完成:
     *   1. I2C/SCCB 总线初始化与配置
     *   2. 遍历 sensor detect 表，通过 SCCB 检测 OV5647
     *   3. 加载 ISP 配置 JSON，创建 V4L2 设备并注册 /dev/video0
     */
    static const esp_video_init_csi_config_t csi_config = {
        .sccb_config = {
            .init_sccb = true,          /* 让 esp_video_init 自动初始化 I2C */
            .i2c_config = {
                .port    = CAM_I2C_PORT,
                .scl_pin = CAM_I2C_SCL_GPIO,
                .sda_pin = CAM_I2C_SDA_GPIO,
            },
            .freq = 400000,             /* 标准模式 400kHz */
        },
        .reset_pin = -1,                /* 无硬件复位引脚 */
        .pwdn_pin  = -1,                /* 无电源控制引脚 */
    };

    static const esp_video_init_config_t video_config = {
        .csi  = &csi_config,             /* MIPI CSI -> /dev/video0 */
        .dvp  = NULL,                    /* 不使用 DVP 接口 */
        .jpeg = NULL,                    /* 不使用 JPEG 硬件编码 */
    };

    esp_err_t ret = esp_video_init(&video_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_video_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ctx->sensor_dev = NULL;  /* sensor 由 esp_video 内部管理，此处仅标记已初始化 */
    ESP_LOGI(TAG, "Camera initialized: %s ready", ESP_VIDEO_MIPI_CSI_DEVICE_NAME);
    return ESP_OK;
}

esp_err_t camera_ov5647_deinit(camera_driver_ctx_t *ctx)
{
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 标记清理：实际 V4L2 资源释放由 video_pipeline_deinit() 通过 close(fd) 完成 */
    ctx->sensor_dev = NULL;
    ESP_LOGI(TAG, "Camera deinitialized");
    return ESP_OK;
}
