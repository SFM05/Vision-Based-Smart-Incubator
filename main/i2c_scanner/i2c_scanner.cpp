#include "i2c_scanner.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "I2C_SCANNER";

I2CScanner::I2CScanner(gpio_num_t sda, gpio_num_t scl, i2c_port_t port, uint32_t clk_hz)
    : m_sda(sda), m_scl(scl), m_port(port), m_clk_hz(clk_hz), m_bus(nullptr), m_owned(true)
{
}

I2CScanner::I2CScanner(i2c_master_bus_handle_t bus, gpio_num_t sda, gpio_num_t scl, i2c_port_t port)
    : m_sda(sda), m_scl(scl), m_port(port),
      m_clk_hz(100000), m_bus(bus), m_owned(false)
{
}

I2CScanner::~I2CScanner()
{
    deinit();
}

esp_err_t I2CScanner::init()
{
    if (m_bus) return ESP_OK;

    i2c_master_bus_config_t cfg = {};
    cfg.i2c_port = m_port;
    cfg.sda_io_num = m_sda;
    cfg.scl_io_num = m_scl;
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.flags.enable_internal_pullup = true;

    return i2c_new_master_bus(&cfg, &m_bus);
}

void I2CScanner::deinit()
{
    if (m_bus && m_owned) {
        i2c_del_master_bus(m_bus);
    }
    m_bus = nullptr;
}

void I2CScanner::set_pins(gpio_num_t sda, gpio_num_t scl)
{
    m_sda = sda;
    m_scl = scl;
}

void I2CScanner::set_port(i2c_port_t port)
{
    m_port = port;
}

void I2CScanner::set_clk_hz(uint32_t hz)
{
    m_clk_hz = hz;
}

bool I2CScanner::probe(uint16_t addr)
{
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = addr;
    dev_cfg.scl_speed_hz = m_clk_hz;
    i2c_master_dev_handle_t dev;
    esp_err_t err = i2c_master_bus_add_device(m_bus, &dev_cfg, &dev);
    if (err != ESP_OK) return false;

    uint8_t dummy;
    err = i2c_master_receive(dev, &dummy, 1, pdMS_TO_TICKS(50));
    i2c_master_bus_rm_device(dev);
    return err == ESP_OK;
}

int I2CScanner::scan()
{
    if (!m_bus) {
        ESP_LOGE(TAG, "Bus not initialized");
        return -1;
    }

    ESP_LOGI(TAG, "Scanning I2C bus %d (SDA=%d, SCL=%d)...", m_port, m_sda, m_scl);

    int found = 0;
    for (uint16_t addr = 1; addr < 0x7F; addr++) {
        if (probe(addr)) {
            ESP_LOGI(TAG, "  Device found at 0x%02X", addr);
            found++;
        }
    }

    if (found == 0) {
        ESP_LOGI(TAG, "No devices found");
    } else {
        ESP_LOGI(TAG, "Scan complete: %d device(s) found", found);
    }

    return found;
}
