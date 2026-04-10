/**
 * @file camera_driver.h
 * @brief 摄像头驱动模块 - OV5647 传感器接口
 *
 * 通过 esp_video_init 框架一键完成 OV5647 初始化，
 * 包括 I2C/SCCB 总线配置、传感器检测、V4L2 设备注册。
 */

#ifndef CAMERA_DRIVER_H
#define CAMERA_DRIVER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 摄像头驱动运行时上下文
 *
 * @note 使用 esp_video_init() 后，sensor 设备由框架内部管理，
 *       此结构体仅作为初始化状态标记使用，不持有实际资源句柄。
 */
typedef struct {
    void *sensor_dev;   ///< 保留字段（esp_video 内部管理 sensor 实例）
} camera_driver_ctx_t;

/**
 * @brief 初始化 OV5647 摄像头
 *
 * 内部调用 esp_video_init() 完成:
 *   - I2C/SCCB 总线初始化 (GPIO7=SDA, GPIO8=SCL, 400kHz)
 *   - OV5647 sensor 自动检测与注册
 *   - V4L2 设备节点创建 (/dev/video0)
 *   - ISP 管道配置（RAW Bayer -> RGB565）
 *
 * 分辨率与帧率由 sdkconfig 中 CONFIG_CAMERA_OV5647_MIPI_DEFAULT_FMT 选择，
 * 当前配置为 1920x1080 RAW10 @30fps（16:9 全高清）。
 *
 * @param[out] ctx 返回的摄像头驱动上下文
 *
 * @return
 *         - ESP_OK: 成功
 *         - ESP_ERR_INVALID_ARG: ctx 为 NULL
 *         - 其他: esp_video_init 失败（I2C 错误 / sensor 未检测到等）
 */
esp_err_t camera_ov5647_init(camera_driver_ctx_t *ctx);

/**
 * @brief 反初始化摄像头模块
 *
 * 当前仅做标记清理。实际的 V4L2 设备关闭在 video_pipeline_deinit()
 * 中通过 close(fd) 完成，避免重复释放。
 *
 * @param[in] ctx 摄像头驱动上下文
 *
 * @return ESP_OK 始终返回成功
 */
esp_err_t camera_ov5647_deinit(camera_driver_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* CAMERA_DRIVER_H */
