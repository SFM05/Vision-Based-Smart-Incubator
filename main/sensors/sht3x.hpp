#pragma once

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "soc/gpio_num.h"
#include <atomic>

namespace sensor
{
    struct SHT3x_data
    {
        float temp;
        float hum;
    };
    class SHT3x
    {
    public:
        static SHT3x &instance()
        {
            static SHT3x m_instance;
            return m_instance;
        }

        void set_cfg(gpio_num_t tx, gpio_num_t rx)
        {
            m_tx = tx;
            m_rx = rx;
        }
        void init();
        SHT3x_data read_data()
        {
            return SHT3x_data{m_temp.load(), m_hum.load()};
        }

        friend void uart_event_task(void *argv);

    private:
        SHT3x() = default;
        SHT3x(const SHT3x &) = delete;
        ~SHT3x() = default;
        SHT3x &operator=(const SHT3x &) = delete;

        static void uart_event_task(void *argv);

        // uart_config_t m_uart_cfg;
        gpio_num_t m_tx;
        gpio_num_t m_rx;
        QueueHandle_t m_queue;
        std::atomic<float> m_temp;
        std::atomic<float> m_hum;
    };
}
