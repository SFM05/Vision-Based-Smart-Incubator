#include "esp_err.h"
#include "esp_log.h"
#include "esp_video_device.h"
#include "esp_video_isp_ioctl.h"
#include "freertos/FreeRTOS.h"
#include "linux/videodev2.h"
#include <sys/mman.h>

#include "cam.hpp"

static const char *TAG = "cam";

namespace CAM
{
    esp_err_t camera::init_camera()
    {
        /**
         * @brief 等待 V4L2 设备注册完成
         */
        vTaskDelay(pdMS_TO_TICKS(200));
        m_fd = open(ESP_VIDEO_MIPI_CSI_DEVICE_NAME, O_RDWR);
        if (m_fd < 0)
        {
            ESP_LOGE(TAG, "Failed to open %s (errno=%d)", ESP_VIDEO_MIPI_CSI_DEVICE_NAME, errno);
            return false;
        }

        struct v4l2_capability cap;
        if (ioctl(m_fd, VIDIOC_QUERYCAP, &cap) < 0)
        {
            ESP_LOGE(TAG, "VIDIOC_QUERYCAP failed");
            close(m_fd);
            m_fd = -1;
            return ESP_FAIL;
        }

        struct v4l2_format fmt;
        memset(&fmt, 0, sizeof(fmt));
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(m_fd, VIDIOC_G_FMT, &fmt) < 0)
        {
            ESP_LOGE(TAG, "VIDIOC_G_FMT failed");
            close(m_fd);
            m_fd = -1;
            return ESP_FAIL;
        }
        m_width = fmt.fmt.pix.width;
        m_height = fmt.fmt.pix.height;
        m_pixfmt = fmt.fmt.pix.pixelformat;
        ESP_LOGI(TAG, "Camera format: %dx%d pixfmt=0x%08x", m_width, m_height, m_pixfmt);

        m_bpp = (m_pixfmt == V4L2_PIX_FMT_RGB24) ? 3 : 2;

        struct v4l2_requestbuffers reqbuf;
        memset(&reqbuf, 0, sizeof(reqbuf));
        reqbuf.count = 2;
        reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        reqbuf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(m_fd, VIDIOC_REQBUFS, &reqbuf) < 0)
        {
            ESP_LOGE(TAG, "REQBUFS failed");
            close(m_fd);
            m_fd = -1;
            return false;
        }

        m_num_bufs = reqbuf.count;
        m_buf_start = (void **)calloc(reqbuf.count, sizeof(void *));
        m_buf_len = (size_t *)calloc(reqbuf.count, sizeof(size_t));
        if (!m_buf_start || !m_buf_len)
        {
            free(m_buf_start);
            free(m_buf_len);
            close(m_fd);
            m_fd = -1;
            return false;
        }

        for (int i = 0; i < reqbuf.count; i++)
        {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            if (ioctl(m_fd, VIDIOC_QUERYBUF, &buf) < 0)
                goto fail;
            m_buf_start[i] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, m_fd, buf.m.offset);
            if (m_buf_start[i] == MAP_FAILED)
                goto fail;
            m_buf_len[i] = buf.length;
        }

        for (int i = 0; i < reqbuf.count; i++)
        {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            ioctl(m_fd, VIDIOC_QBUF, &buf);
        }

        {
            enum v4l2_buf_type bt = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (ioctl(m_fd, VIDIOC_STREAMON, &bt) < 0)
                goto fail;
        }
        m_streaming = true;
        ESP_LOGI(TAG, "Camera started");
        return ESP_OK;

    fail:
        for (int i = 0; i < reqbuf.count; i++)
            if (m_buf_start[i] && m_buf_start[i] != MAP_FAILED)
                munmap(m_buf_start[i], m_buf_len[i]);
        free(m_buf_start);
        free(m_buf_len);
        close(m_fd);
        m_fd = -1;
        return ESP_FAIL;
    }

    esp_err_t camera::init_dsp()
    {
        int fd = open(ESP_VIDEO_ISP1_DEVICE_NAME, O_RDWR);
        if (fd < 0)
        {
            ESP_LOGE(TAG, "Cannot open ISP device %s: %s",
                     ESP_VIDEO_ISP1_DEVICE_NAME, strerror(errno));
            return ESP_FAIL;
        }

        v4l2_ext_control ctrl{};
        // memset(&ctrl, 0, sizeof(ctrl));

        v4l2_ext_controls ctrls{};
        // memset(&ctrls, 0, sizeof(ctrls));
        ctrls.count = 1;
        ctrls.controls = &ctrl;

        esp_video_isp_bf_t bf;
        memset(&bf, 0, sizeof(bf));
        bf.enable = true;
        bf.level = 15;
        bf.matrix[0][0] = 1;
        bf.matrix[0][1] = 2;
        bf.matrix[0][2] = 1;
        bf.matrix[1][0] = 2;
        bf.matrix[1][1] = 4;
        bf.matrix[1][2] = 2;
        bf.matrix[2][0] = 1;
        bf.matrix[2][1] = 2;
        bf.matrix[2][2] = 1;

        ctrl.id = V4L2_CID_USER_ESP_ISP_BF;
        ctrl.size = sizeof(bf);
        // ctrl.p_u8 = (__u8 *)&bf;
        ctrl.p_u8 = reinterpret_cast<__u8 *>(&bf);

        if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls) < 0)
            ESP_LOGW(TAG, "ISP BF failed: %s", strerror(errno));
        else
            ESP_LOGI(TAG, "ISP BF level=%d", bf.level);

        // esp_isp_crop_config_t crop_cfg={
        //     {{420,0},{1499,1079}}
        // };
        // esp_isp_crop_configure(isp_proc_handle_t isp_proc, const esp_isp_crop_config_t *config)

        close(fd);
        return ESP_OK;
    }
}
