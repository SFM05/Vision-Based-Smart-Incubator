#pragma once
#include "esp_err.h"
#include <cstddef>
#include <cstdint>

namespace CAM
{
    class camera
    {
    public:
        static camera &instance()
        {
            static camera m_instance;
            return m_instance;
        }

        esp_err_t init_camera();
        esp_err_t init_dsp();

        int fd() { return m_fd; }
        void **buf_start() { return m_buf_start; }
        size_t *buf_len() { return m_buf_len; }
        int num_bufs() { return m_num_bufs; }
        uint16_t width() { return m_width; }
        uint16_t height() { return m_height; }
        uint32_t pixfmt() { return m_pixfmt; }
        int bpp() { return m_bpp; }
        bool streaming() { return m_streaming; }
        int last_index() { return m_last_index; }

    private:
        camera() : m_fd(-1) {}
        camera(const camera &) = delete;
        ~camera() = default;
        camera &operator=(const camera &) = delete;

        int m_fd;
        void **m_buf_start;
        size_t *m_buf_len;
        int m_num_bufs;
        uint16_t m_width;
        uint16_t m_height;
        uint32_t m_pixfmt;
        int m_bpp;
        bool m_streaming;
        int m_last_index;
    };
}