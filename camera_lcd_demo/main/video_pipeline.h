/**
 * @file video_pipeline.h
 * @brief 视频管道模块 - V4L2 帧采集与 LCD 显示
 *
 * 基于 esp_video 的 V4L2 接口实现摄像头帧采集，
 * 支持最近邻缩放至 LCD 全屏分辨率并直接 DMA 输出到 MIPI DSI 显示。
 */

#ifndef VIDEO_PIPELINE_H
#define VIDEO_PIPELINE_H

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 视频管道初始化配置
 */
typedef struct {
    uint16_t width;          ///< 期望的图像宽度（像素）
    uint16_t height;         ///< 期望的图像高度（像素）
    uint8_t  num_buffers;    ///< 帧缓冲区数量（建议 2~3）
} video_pipeline_config_t;

/**
 * @brief 视频管道运行时上下文
 *
 * @note 该结构体在 app_main() 中必须声明为 static，
 *       因为 preview_task 在 app_main() 返回后仍需访问它。
 */
typedef struct {
    int    video_fd;                     ///< V4L2 设备文件描述符 (/dev/video0)
    void **buffer_start;                 ///< MMAP 帧缓冲区起始地址数组
    size_t *buffer_length;               ///< MMAP 帧缓冲区长度数组
    uint8_t num_buffers;                 ///< 实际分配的帧缓冲区数量
    esp_lcd_panel_handle_t panel_handle; ///< LCD 面板句柄（用于 draw_bitmap）
    uint32_t frame_pixel_format;         ///< 实际帧像素格式 (V4L2_PIX_FMT_*)
    bool     is_streaming;              ///< 当前是否处于 streaming 状态
} video_pipeline_ctx_t;

/**
 * @brief 初始化视频管道
 *
 * 执行以下步骤：
 *   1. 打开 V4L2 视频设备
 *   2. 查询设备能力与传感器原生格式
 *   3. 协商像素格式（优先 RGB565，回退 RGB24 或原生格式）
 *   4. 通过 REQBUFS + MMAP 分配帧缓冲区
 *   5. 将所有缓冲区入队（QBUF），等待 STREAMON 后填充
 *
 * @param[in]  config        管道配置参数（宽度、高度、缓冲数量）
 * @param[in]  panel_handle  已初始化的 LCD 面板句柄
 * @param[out] pipeline_ctx  输出的管道上下文（调用方负责生命周期管理）
 *
 * @return ESP_OK 成功, 其他值表示失败
 */
esp_err_t video_pipeline_init(
    const video_pipeline_config_t *config,
    esp_lcd_panel_handle_t panel_handle,
    video_pipeline_ctx_t *pipeline_ctx
);

/**
 * @brief 启动视频流采集 (STREAMON)
 *
 * @param[in] pipeline_ctx 已初始化的视频管道上下文
 *
 * @return ESP_OK 成功, ESP_ERR_INVALID_STATE 参数无效或已启动
 */
esp_err_t video_pipeline_start(video_pipeline_ctx_t *pipeline_ctx);

/**
 * @brief 停止视频流采集 (STREAMOFF)
 *
 * @param[in] pipeline_ctx 视频管道上下文
 *
 * @return ESP_OK 成功, ESP_ERR_INVALID_STATE 参数无效或未启动
 */
esp_err_t video_pipeline_stop(video_pipeline_ctx_t *pipeline_ctx);

/**
 * @brief 反初始化视频管道，释放所有资源
 *
 * 内部会先执行 stop（如果正在 streaming），
 * 然后 munmap 所有缓冲区、释放数组、关闭 fd。
 *
 * @param[in] pipeline_ctx 视频管道上下文
 *
 * @return ESP_OK 成功, ESP_ERR_INVALID_ARG 参数为 NULL
 */
esp_err_t video_pipeline_deinit(video_pipeline_ctx_t *pipeline_ctx);

/**
 * @brief FreeRTOS 预览任务入口函数
 *
 * 主循环逻辑：
 *   - DQBUF（阻塞等待新帧）
 *   - 最近邻缩放至 LCD 全屏分辨率
 *   - esp_lcd_panel_draw_bitmap DMA 传输到屏幕
 *   - QBUF 归还缓冲区
 *
 * @param arg video_pipeline_ctx_t 指针（通过 xTaskCreate 传入）
 *
 * @note 此函数不会返回；出错时自行 vTaskDelete。
 */
void video_preview_task(void *arg);

#ifdef __cplusplus
}
#endif

#endif /* VIDEO_PIPELINE_H */
