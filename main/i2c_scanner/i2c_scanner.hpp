#pragma once

#include "driver/i2c_master.h"

class I2CScanner {
public:
    I2CScanner(gpio_num_t sda = GPIO_NUM_7, gpio_num_t scl = GPIO_NUM_8,
               i2c_port_t port = I2C_NUM_0, uint32_t clk_hz = 100000);

    I2CScanner(i2c_master_bus_handle_t bus, gpio_num_t sda, gpio_num_t scl, i2c_port_t port = I2C_NUM_0);

    ~I2CScanner();

    esp_err_t init();
    void deinit();

    void set_pins(gpio_num_t sda, gpio_num_t scl);
    void set_port(i2c_port_t port);
    void set_clk_hz(uint32_t hz);

    int scan();
    bool probe(uint16_t addr);

private:
    gpio_num_t m_sda;
    gpio_num_t m_scl;
    i2c_port_t m_port;
    uint32_t m_clk_hz;
    i2c_master_bus_handle_t m_bus;
    bool m_owned;
};
