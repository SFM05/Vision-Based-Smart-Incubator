#include "driver/uart.h"
#include "esp_err.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "hal/uart_types.h"
#include "soc/gpio_num.h"
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "sht3x.hpp"

namespace sensor
{
    constexpr uart_port_t UART_PORT = UART_NUM_1;
    constexpr int BUF_SIZE = 512;
    void SHT3x::init()
    {
        uart_config_t uart_cfg{
            9600,
            UART_DATA_8_BITS,
            UART_PARITY_DISABLE,
            UART_STOP_BITS_1,
            UART_HW_FLOWCTRL_DISABLE,
            UART_SCLK_DEFAULT};
        uart_driver_install(UART_PORT,
                            BUF_SIZE * 2,
                            0,
                            5,
                            &m_queue,
                            0);
        uart_param_config(UART_PORT, &uart_cfg);
        uart_set_pin(UART_PORT,
                     m_tx,
                     m_rx,
                     UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE);

        vTaskDelay(pdMS_TO_TICKS(10));
        const char instruction[] = "AUTO\r\n";
        uart_write_bytes(UART_PORT, instruction, sizeof(instruction));

        // 启动读取数据的任务
        xTaskCreate(uart_event_task,
                    "UART Receive",
                    1024,
                    nullptr,
                    2,
                    nullptr);
    }

    void SHT3x::uart_event_task(void *argv)
    {
        uart_event_t event;
        uint8_t dtmp[BUF_SIZE];
        float temp;
        float hum;

        while (true)
        {
            if (xQueueReceive(instance().m_queue,
                              static_cast<void *>(&event),
                              portMAX_DELAY))
            {
                if (event.type == UART_DATA)
                {
                    uart_read_bytes(UART_PORT,
                                    dtmp,
                                    event.size,
                                    portMAX_DELAY);
                    // 处理接收到的数据
                    sscanf(reinterpret_cast<char *>(dtmp),
                           "R%fRH %fC\r\n", &hum, &temp);
                    instance().m_hum.store(hum);
                    instance().m_temp.store(temp);
                }
            }
        }
    }
}