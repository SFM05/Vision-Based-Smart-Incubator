/**
 * @file video_pipeline.c
 * @brief 视频管道模块实现 - V4L2 帧采集、缩放与 LCD 显示
 *
 * 硬件平台: ESP32-P4
 * 摄像头:  OV5647 (MIPI CSI, 1920x1080 RAW10 -> ISP RGB565)
 * 显示屏:  EK79007 (MIPI DSI, 1024x600 RGB565)
 */

#include "video_pipeline.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "linux/videodev2.h"
#include "esp_video_device.h"
#include <sys/mman.h>

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

static const char *TAG = "video_pipeline";

/* ---------- 宏定义 ---------- */

/** V4L2 设备路径（esp_video 框架注册的 MIPI CSI 节点） */
#define VIDEO_DEVICE_PATH   ESP_VIDEO_MIPI_CSI_DEVICE_NAME

/** LCD 物理分辨率 */
#define LCD_WIDTH           1024
#define LCD_HEIGHT          600

/* ---------- 模块全局变量（init 阶段确定，preview_task 只读访问） ---------- */

static uint16_t g_frame_width;   ///< 实际帧宽度（来自传感器或 S_FMT 协商结果）
static uint16_t g_frame_height;  ///< 实际帧高度
static uint32_t g_frame_pixfmt;  ///< V4L2 像素格式 fourcc
static uint8_t  g_frame_bpp;     ///< 每像素字节数: 2=RGB565, 3=RGB888, 4=RGBX32

/* ========================= 公共接口实现 ========================= */

esp_err_t video_pipeline_init(
    const video_pipeline_config_t *config,
    esp_lcd_panel_handle_t panel_handle,
    video_pipeline_ctx_t *pipeline_ctx)
{
    if (config == NULL || pipeline_ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing video pipeline");

    memset(pipeline_ctx, 0, sizeof(video_pipeline_ctx_t));
    pipeline_ctx->panel_handle = panel_handle;

    /* ---- Step 1: 打开 V4L2 视频设备 ---- */
    ESP_LOGI(TAG, "Opening video device: %s", VIDEO_DEVICE_PATH);
    pipeline_ctx->video_fd = open(VIDEO_DEVICE_PATH, O_RDWR);
    if (pipeline_ctx->video_fd < 0) {
        ESP_LOGE(TAG, "Failed to open video device: %d", pipeline_ctx->video_fd);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Video device opened (fd=%d)", pipeline_ctx->video_fd);

    /* ---- Step 2: 查询设备能力 ---- */
    struct v4l2_capability cap;
    memset(&cap, 0, sizeof(cap));
    if (ioctl(pipeline_ctx->video_fd, VIDIOC_QUERYCAP, &cap) < 0) {
        ESP_LOGE(TAG, "Failed to query capabilities");
        close(pipeline_ctx->video_fd);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "V4L2 device: %.32s", cap.card);

    /* ---- Step 3: 获取传感器原生格式并尝试协商输出格式 ---- */
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    ioctl(pipeline_ctx->video_fd, VIDIOC_G_FMT, &fmt);
    uint32_t native_w  = fmt.fmt.pix.width;
    uint32_t native_h  = fmt.fmt.pix.height;
    uint32_t native_fmt = fmt.fmt.pix.pixelformat;
    ESP_LOGI(TAG, "Sensor native format: %dx%d fourcc=%.4s",
             native_w, native_h, (char *)&native_fmt);

    /* 尝试设置期望的分辨率和像素格式 */
    fmt.fmt.pix.width       = config->width;
    fmt.fmt.pix.height      = config->height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
    fmt.fmt.pix.field       = V4L2_FIELD_NONE;

    if (ioctl(pipeline_ctx->video_fd, VIDIOC_S_FMT, &fmt) < 0) {
        /* S_FMT 被拒绝 → 回退到原生分辨率，仅尝试更改像素格式 */
        ESP_LOGW(TAG, "S_FMT(%dx%d RGB565) rejected, using native %dx%d %.4s",
                 config->width, config->height, native_w, native_h, (char *)&native_fmt);
        g_frame_width  = native_w;
        g_frame_height = native_h;
        g_frame_pixfmt = native_fmt;

        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(pipeline_ctx->video_fd, VIDIOC_G_FMT, &fmt);

        const uint32_t try_fmts[] = { V4L2_PIX_FMT_RGB565, V4L2_PIX_FMT_RGB24 };
        bool converted = false;
        for (int i = 0; i < 2; i++) {
            fmt.fmt.pix.pixelformat = try_fmts[i];
            fmt.fmt.pix.field       = V4L2_FIELD_NONE;
            if (ioctl(pipeline_ctx->video_fd, VIDIOC_S_FMT, &fmt) == 0) {
                g_frame_pixfmt = try_fmts[i];
                converted = true;
                ESP_LOGI(TAG, "Format converted: %dx%d %.4s", native_w, native_h, (char *)&try_fmts[i]);
                break;
            }
        }
        if (!converted) {
            ESP_LOGW(TAG, "Keeping native pixel format: %.4s", (char *)&native_fmt);
        }
    } else {
        g_frame_width  = fmt.fmt.pix.width;
        g_frame_height = fmt.fmt.pix.height;
        g_frame_pixfmt = fmt.fmt.pix.pixelformat;
        ESP_LOGI(TAG, "Format set: %dx%d %.4s", g_frame_width, g_frame_height, (char *)&g_frame_pixfmt);
    }

    /*
     * Step 3.5: 根据像素格式计算 BPP
     *
     * OV5647 + ISP 管道说明:
     *   - 每个 format 配置都包含 isp_info（bypass_isp = false）
     *   - ISP 将 RAW Bayer 数据处理为标准 RGB565（2 bytes/pixel）
     *   - 'RGBP' fourcc 即 V4L2_PIX_FMT_RGB565
     *   - sizeimage=0 是 ESP 驱动在 ISP 模式下的已知行为，不代表数据异常
     */
    uint32_t si = fmt.fmt.pix.sizeimage;
    ESP_LOGI(TAG, "sizeimage=%u, pixelformat=0x%08x (%.4s)",
             si, g_frame_pixfmt, (char *)&g_frame_pixfmt);

    if (g_frame_pixfmt == V4L2_PIX_FMT_RGB565) {
        g_frame_bpp = 2;
        ESP_LOGI(TAG, "Format: standard RGB565 (2 BPP) from ISP pipeline");
    } else if (g_frame_pixfmt == V4L2_PIX_FMT_RGB24) {
        g_frame_bpp = 3;
        ESP_LOGI(TAG, "Format: RGB24 (3 BPP)");
    } else if (g_frame_pixfmt == V4L2_PIX_FMT_BGR24) {
        g_frame_bpp = 3;
        ESP_LOGI(TAG, "Format: BGR24 (3 BPP)");
    } else if (g_frame_pixfmt == V4L2_PIX_FMT_RGBX32 ||
               g_frame_pixfmt == V4L2_PIX_FMT_XBGR32) {
        g_frame_bpp = 4;
        ESP_LOGI(TAG, "Format: RGBX32/XBGR32 (4 BPP)");
    } else {
        g_frame_bpp = 2;  /* 默认按 2BPP 处理 */
        ESP_LOGW(TAG, "Unknown pixfmt 0x%08x (%.4s), defaulting to 2 BPP",
                 g_frame_pixfmt, (char *)&g_frame_pixfmt);
    }

    pipeline_ctx->frame_pixel_format = g_frame_pixfmt;

    /* ---- Step 4: 请求帧缓冲区（REQBUFS）---- */
    struct v4l2_requestbuffers reqbuf;
    memset(&reqbuf, 0, sizeof(reqbuf));
    reqbuf.count  = config->num_buffers;
    reqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(pipeline_ctx->video_fd, VIDIOC_REQBUFS, &reqbuf) < 0) {
        ESP_LOGE(TAG, "Failed to request buffers");
        close(pipeline_ctx->video_fd);
        return ESP_FAIL;
    }

    /*
     * 关键: ESP V4L2 驱动的 REQBUFS 不回写 reqbuf.count（与 Linux 行为不同）
     * 驱动内部使用传入的 count 值分配缓冲区，但不修改该字段，
     * 因此直接使用请求值作为实际缓冲区数量。
     */
    pipeline_ctx->num_buffers = config->num_buffers;
    ESP_LOGI(TAG, "Buffers requested: %d (ESP V4L2 does not update req.count)", config->num_buffers);

    /* 分配缓冲区元数据数组（起始地址指针 + 长度） */
    pipeline_ctx->buffer_start  = calloc(reqbuf.count, sizeof(void *));
    pipeline_ctx->buffer_length = calloc(reqbuf.count, sizeof(size_t));
    if (!pipeline_ctx->buffer_start || !pipeline_ctx->buffer_length) {
        ESP_LOGE(TAG, "Failed to allocate buffer arrays");
        close(pipeline_ctx->video_fd);
        free(pipeline_ctx->buffer_start);
        free(pipeline_ctx->buffer_length);
        return ESP_ERR_NO_MEM;
    }

    /* ---- Step 5: MMAP 映射每个缓冲区 ---- */
    for (int i = 0; i < reqbuf.count; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;

        if (ioctl(pipeline_ctx->video_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            ESP_LOGE(TAG, "Failed to query buffer %d", i);
            goto cleanup_bufs;
        }

        pipeline_ctx->buffer_start[i] = mmap(NULL, buf.length,
                                             PROT_READ | PROT_WRITE,
                                             MAP_SHARED,
                                             pipeline_ctx->video_fd, buf.m.offset);
        if (pipeline_ctx->buffer_start[i] == MAP_FAILED) {
            ESP_LOGE(TAG, "Failed to mmap buffer %d", i);
            goto cleanup_bufs;
        }
        pipeline_ctx->buffer_length[i] = buf.length;
        ESP_LOGI(TAG, "Buffer %d: addr=%p len=%zu (expected %d for %dBPP)",
                 i, pipeline_ctx->buffer_start[i], buf.length,
                 g_frame_width * g_frame_height * g_frame_bpp, g_frame_bpp);
    }

    /* ---- Step 6: 将所有缓冲区入队（QBUF），等待 STREAMON 后填充 ---- */
    for (int i = 0; i < reqbuf.count; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        if (ioctl(pipeline_ctx->video_fd, VIDIOC_QBUF, &buf) < 0) {
            ESP_LOGE(TAG, "Failed to queue buffer %d", i);
        }
    }

    ESP_LOGI(TAG, "Video pipeline ready: %dx%d %.4s (%d BPP), %d bufs",
             g_frame_width, g_frame_height, (char *)&g_frame_pixfmt,
             g_frame_bpp, reqbuf.count);
    return ESP_OK;

cleanup_bufs:
    for (int j = 0; j < reqbuf.count; j++) {
        if (pipeline_ctx->buffer_start[j] && pipeline_ctx->buffer_start[j] != MAP_FAILED) {
            munmap(pipeline_ctx->buffer_start[j], pipeline_ctx->buffer_length[j]);
        }
    }
    free(pipeline_ctx->buffer_start);
    free(pipeline_ctx->buffer_length);
    close(pipeline_ctx->video_fd);
    return ESP_FAIL;
}

esp_err_t video_pipeline_start(video_pipeline_ctx_t *pipeline_ctx)
{
    if (!pipeline_ctx || pipeline_ctx->video_fd < 0) {
        return ESP_ERR_INVALID_STATE;
    }
    if (pipeline_ctx->is_streaming) {
        return ESP_OK;  /* 幂等: 已启动则直接返回成功 */
    }

    ESP_LOGI(TAG, "Starting video stream");
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(pipeline_ctx->video_fd, VIDIOC_STREAMON, &type) < 0) {
        ESP_LOGE(TAG, "STREAMON failed");
        return ESP_FAIL;
    }
    pipeline_ctx->is_streaming = true;
    ESP_LOGI(TAG, "Streaming started");
    return ESP_OK;
}

esp_err_t video_pipeline_stop(video_pipeline_ctx_t *pipeline_ctx)
{
    if (!pipeline_ctx || pipeline_ctx->video_fd < 0) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!pipeline_ctx->is_streaming) {
        return ESP_OK;  /* 幂等: 已停止则直接返回成功 */
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(pipeline_ctx->video_fd, VIDIOC_STREAMOFF, &type);
    pipeline_ctx->is_streaming = false;
    ESP_LOGI(TAG, "Streaming stopped");
    return ESP_OK;
}

esp_err_t video_pipeline_deinit(video_pipeline_ctx_t *pipeline_ctx)
{
    if (!pipeline_ctx) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 先停止 streaming（如果正在运行） */
    if (pipeline_ctx->is_streaming) {
        video_pipeline_stop(pipeline_ctx);
    }

    /* 解除 MMAP 映射并释放数组 */
    if (pipeline_ctx->buffer_start) {
        for (int i = 0; i < pipeline_ctx->num_buffers; i++) {
            if (pipeline_ctx->buffer_start[i] && pipeline_ctx->buffer_start[i] != MAP_FAILED) {
                munmap(pipeline_ctx->buffer_start[i], pipeline_ctx->buffer_length[i]);
            }
        }
        free(pipeline_ctx->buffer_start);
        pipeline_ctx->buffer_start = NULL;
    }
    free(pipeline_ctx->buffer_length);
    pipeline_ctx->buffer_length = NULL;

    /* 关闭 V4L2 设备文件描述符 */
    if (pipeline_ctx->video_fd >= 0) {
        close(pipeline_ctx->video_fd);
        pipeline_ctx->video_fd = -1;
    }

    ESP_LOGI(TAG, "Pipeline deinitialized");
    return ESP_OK;
}

/* ========================= 内部辅助函数 ========================= */

/**
 * @brief 将 RGB888 像素数据转换为 RGB565
 *
 * @param[in]  src              源数据起始地址
 * @param[in]  src_stride       源数据行步幅（字节），通常为 width * 3
 * @param[out] dst              输出缓冲区起始地址（uint16_t 数组）
 * @param[in]  dst_stride_pixels 输出行步幅（像素数），通常为 width
 * @param[in]  width            图像宽度（像素）
 * @param[in]  height           图像高度（像素）
 */
static void convert_rgb888_to_rgb565(
    const uint8_t *src, int src_stride,
    uint16_t *dst, int dst_stride_pixels,
    int width, int height)
{
    for (int y = 0; y < height; y++) {
        const uint8_t *src_row = src + y * src_stride;
        uint16_t *dst_row = dst + y * dst_stride_pixels;
        for (int x = 0; x < width; x++) {
            uint8_t r = src_row[x * 3 + 0];
            uint8_t g = src_row[x * 3 + 1];
            uint8_t b = src_row[x * 3 + 2];
            dst_row[x] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        }
    }
}

/**
 * @brief RGB565 字节序交换（Little-Endian -> Big-Endian）
 *
 * ESP32-P4 CSI 输出为 LE 排序:
 *   [低字节 G[2:0]|B[7:3]] [高字节 R[7:3]|G[5:3]]
 * LCD DPI 引擎期望 BE 排序:
 *   [高字节 R[7:3]|G[5:3]] [低字节 G[2:0]|B[7:3]]
 *
 * @note 当前版本未启用此函数；DPI 引擎配置正确后硬件自动处理字节序。
 *
 * @param[in]  src          源数据地址
 * @param[out] dst          输出数据地址
 * @param[in]  total_bytes  总字节数（必须为偶数）
 */
static void swap_rgb565_endian(const uint8_t *src, uint8_t *dst, size_t total_bytes)
{
    for (size_t i = 0; i < total_bytes; i += 2) {
        dst[i]     = src[i + 1];  /* 高字节 -> 低地址 */
        dst[i + 1] = src[i];      /* 低字节 -> 高地址 */
    }
}

/* ========================= FreeRTOS 预览任务 ========================= */

void video_preview_task(void *arg)
{
    if (!arg) {
        vTaskDelete(NULL);
        return;
    }

    video_pipeline_ctx_t *ctx = (video_pipeline_ctx_t *)arg;
    ESP_LOGI(TAG, "Preview task started: frame=%dx%d %.4s (%dBPP) -> LCD %dx%d",
             g_frame_width, g_frame_height, (char *)&g_frame_pixfmt, g_frame_bpp,
             LCD_WIDTH, LCD_HEIGHT);

    /* 计算源/目标尺寸：输出始终为 LCD 全屏分辨率 */
    const uint16_t fw = g_frame_width;
    const uint16_t fh = g_frame_height;
    const uint16_t draw_w = LCD_WIDTH;
    const uint16_t draw_h = LCD_HEIGHT;

    ESP_LOGI(TAG, "Frame %dx%d -> scaled to fullscreen %dx%d", fw, fh, draw_w, draw_h);

    /* 判断是否需要颜色空间转换 */
    const bool need_color_convert = (g_frame_bpp != 2);  /* 非 RGB565 需要转 RGB565 */
    ESP_LOGI(TAG, "Processing: color_convert=%s (source=%d BPP), direct DMA to LCD",
             need_color_convert ? "YES" : "no", g_frame_bpp);

    /*
     * 分配全屏帧缓冲区（LCD_WIDTH * LCD_HEIGHT * 2 bytes/pixel RGB565）
     * 优先从 PSRAM 分配（大容量），回退到内部 SRAM
     */
    uint8_t *convert_buf = NULL;
    const size_t convert_buf_size = (size_t)LCD_WIDTH * LCD_HEIGHT * 2;

    convert_buf = heap_caps_malloc(convert_buf_size, MALLOC_CAP_SPIRAM);
    if (!convert_buf) {
        convert_buf = malloc(convert_buf_size);
    }
    if (!convert_buf) {
        ESP_LOGE(TAG, "Failed to allocate conversion buffer (%zu bytes)", convert_buf_size);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Conversion buffer allocated: %zu bytes at %p", convert_buf_size, convert_buf);
    ESP_LOGI(TAG, "Starting camera preview...");

    /* ========== 主预览循环: DQBUF -> 缩放 -> draw_bitmap -> QBUF ========== */
    while (1) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        /* 出队一帧（阻塞等待直到有新帧可用） */
        if (ioctl(ctx->video_fd, VIDIOC_DQBUF, &buf) < 0) {
            ESP_LOGE(TAG, "DQBUF failed");
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /* 安全检查: 确保 buffer index 在有效范围内 */
        if (buf.index >= ctx->num_buffers) {
            ESP_LOGE(TAG, "Invalid buf.index=%d >= num_buffers=%d", buf.index, ctx->num_buffers);
            ioctl(ctx->video_fd, VIDIOC_QBUF, &buf);  /* 仍需入队以避免泄漏 */
            continue;
        }

        uint8_t *frame_data = (uint8_t *)ctx->buffer_start[buf.index];

        /* 诊断日志: 仅前 3 帧打印原始数据用于调试 */
        static int diag_cnt = 0;
        if (diag_cnt < 3) {
            ESP_LOGI(TAG, "Frame[%d]: index=%d bytesused=%d buf_len=%zu",
                     diag_cnt, buf.index, buf.bytesused, ctx->buffer_length[buf.index]);
            ESP_LOGI(TAG, "First 16 bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                     frame_data[0], frame_data[1], frame_data[2], frame_data[3],
                     frame_data[4], frame_data[5], frame_data[6], frame_data[7],
                     frame_data[8], frame_data[9], frame_data[10], frame_data[11],
                     frame_data[12], frame_data[13], frame_data[14], frame_data[15]);
            ESP_LOGI(TAG, "Scale: %dx%d -> %dx%d, bpp=%d", fw, fh, draw_w, draw_h, g_frame_bpp);
            diag_cnt++;
        }

        /* 最近邻缩放至 LCD 全屏分辨率 */
        if (g_frame_bpp == 2) {
            /*
             * RGB565 路径: 直接逐像素复制 + 最近邻缩放
             * DPI 引擎已配置为正确的像素格式，无需软件层字节序交换
             */
            const uint16_t *src = (const uint16_t *)frame_data;
            uint16_t *dst = (uint16_t *)convert_buf;
            for (int y = 0; y < draw_h; y++) {
                int sy = (y * fh) / draw_h;                    /* 源 Y 坐标（最近邻） */
                const uint16_t *src_row = src + sy * fw;
                uint16_t *dst_row = dst + y * draw_w;
                for (int x = 0; x < draw_w; x++) {
                    int sx = (x * fw) / draw_w;                /* 源 X 坐标（最近邻） */
                    dst_row[x] = src_row[sx];
                }
            }
        } else if (g_frame_bpp == 3) {
            /*
             * RGB888/BGR24 路径: 颜色空间转换 (RGB888 -> RGB565) + 最近邻缩放
             */
            const uint8_t *src = frame_data;
            uint16_t *dst = (uint16_t *)convert_buf;
            for (int y = 0; y < draw_h; y++) {
                int sy = (y * fh) / draw_h;
                const uint8_t *src_row = src + sy * fw * 3;
                uint16_t *dst_row = dst + y * draw_w;
                for (int x = 0; x < draw_w; x++) {
                    int sx = (x * fw) / draw_w;
                    int si = sx * 3;
                    uint8_t r = src_row[si];
                    uint8_t g = src_row[si + 1];
                    uint8_t b = src_row[si + 2];
                    dst_row[x] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                }
            }
        }

        /* DMA 传输至 LCD 屏幕 */
        esp_lcd_panel_draw_bitmap(ctx->panel_handle, 0, 0, draw_w, draw_h, convert_buf);

        /* 归还缓冲区，驱动重新填充下一帧 */
        ioctl(ctx->video_fd, VIDIOC_QBUF, &buf);
    }
}
