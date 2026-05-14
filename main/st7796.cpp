#include "st7796.hpp"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "st7796";

static spi_device_handle_t spi_dev;

/* ------------------------------------------------------------------ */
/*  Low-level SPI helpers  (caller must hold bus if CS must stay low) */
/* ------------------------------------------------------------------ */
static void write_cmd(uint8_t cmd)
{
    spi_transaction_t t{};
    t.length    = 8;
    t.rxlength  = 0;
    t.tx_buffer = &cmd;
    gpio_set_level((gpio_num_t)PIN_LCD_DC, 0);
    spi_device_polling_transmit(spi_dev, &t);
}

static void write_data_buf(const uint8_t *data, size_t len)
{
    if (len == 0) return;
    spi_transaction_t t{};
    t.length    = len * 8;
    t.rxlength  = 0;
    t.tx_buffer = data;
    gpio_set_level((gpio_num_t)PIN_LCD_DC, 1);
    spi_device_polling_transmit(spi_dev, &t);
}

static void write_data_byte(uint8_t data)
{
    write_data_buf(&data, 1);
}

static void write_data_word(uint16_t data)
{
    uint8_t buf[2] = { uint8_t(data >> 8), uint8_t(data & 0xFF) };
    write_data_buf(buf, 2);
}

/* ------------------------------------------------------------------ */
/*  Init sequence  (LCDWiki — ST7796S TN panel)                       */
/* ------------------------------------------------------------------ */
static void lcd_reset(void)
{
    gpio_set_level((gpio_num_t)PIN_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level((gpio_num_t)PIN_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

static void lcd_init(void)
{
    ESP_LOGI(TAG, "lcd_init start");
    lcd_reset();

    spi_device_acquire_bus(spi_dev, portMAX_DELAY);
    write_cmd(0x11);   /* Sleep Out */
    spi_device_release_bus(spi_dev);
    vTaskDelay(pdMS_TO_TICKS(120));

    spi_device_acquire_bus(spi_dev, portMAX_DELAY);
    write_cmd(0x36); write_data_byte(0xE8);  /* MADCTL: MV=1 MX=1 MY=1 BGR=1 */
    write_cmd(0x3A); write_data_byte(0x05);  /* RGB565 */
    write_cmd(0x29);   /* Display ON */
    spi_device_release_bus(spi_dev);
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "init done");
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */
void st7796_init(void)
{
    if (PIN_LCD_LED >= 0) {
        gpio_set_direction((gpio_num_t)PIN_LCD_LED, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)PIN_LCD_LED, 1);
    }
    gpio_set_direction((gpio_num_t)PIN_LCD_RST, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)PIN_LCD_RST, 1);
    gpio_set_direction((gpio_num_t)PIN_LCD_DC, GPIO_MODE_OUTPUT);
    /* CS is controlled by SPI hardware via spics_io_num — no manual GPIO init */

    spi_bus_config_t bus_cfg{};
    bus_cfg.mosi_io_num     = PIN_LCD_MOSI;
    bus_cfg.miso_io_num     = PIN_LCD_MISO;
    bus_cfg.sclk_io_num     = PIN_LCD_SCLK;
    bus_cfg.quadwp_io_num   = -1;
    bus_cfg.quadhd_io_num   = -1;
    bus_cfg.max_transfer_sz = LVGL_HOR_RES * LVGL_VER_RES * 2 + 8;

    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    /* Enable internal pull-up on MISO to prevent floating input */
    if (PIN_LCD_MISO >= 0) {
        gpio_set_pull_mode((gpio_num_t)PIN_LCD_MISO, GPIO_PULLUP_ONLY);
    }

    spi_device_interface_config_t dev_cfg{};
    dev_cfg.clock_speed_hz = 40 * 1000 * 1000;
    dev_cfg.mode           = 0;
    dev_cfg.spics_io_num   = PIN_LCD_CS;
    dev_cfg.queue_size     = 2;
    dev_cfg.flags          = 0;

    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev_cfg, &spi_dev));

    lcd_init();
}

static uint16_t swap16(uint16_t c) {
    return (c >> 8) | (c << 8);
}

void st7796_fill_screen(uint16_t color)
{
    /* Fill full screen */
    color = swap16(color);
    uint16_t buf[256];
    for (int i = 0; i < 256; i++) buf[i] = color;
    const int total = ST7796_LCD_W * ST7796_LCD_H;

    spi_device_acquire_bus(spi_dev, portMAX_DELAY);
    write_cmd(0x2A); write_data_word(0); write_data_word(ST7796_LCD_W - 1);
    write_cmd(0x2B); write_data_word(0); write_data_word(ST7796_LCD_H - 1);
    write_cmd(0x2C);
    int sent = 0;
    while (sent < total) {
        int n = total - sent;
        if (n > 256) n = 256;
        write_data_buf(reinterpret_cast<const uint8_t *>(buf), n * 2);
        sent += n;
    }
    spi_device_release_bus(spi_dev);
}

void st7796_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    int x2 = x + w - 1;
    int y2 = y + h - 1;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 >= LVGL_HOR_RES) x2 = LVGL_HOR_RES - 1;
    if (y2 >= LVGL_VER_RES) y2 = LVGL_VER_RES - 1;
    if (x > x2 || y > y2) return;

    int total = (x2 - x + 1) * (y2 - y + 1);

    color = swap16(color);
    uint16_t buf[256];
    for (int i = 0; i < 256; i++) buf[i] = color;

    spi_device_acquire_bus(spi_dev, portMAX_DELAY);
    write_cmd(0x2A); write_data_word(uint16_t(x)); write_data_word(uint16_t(x2));
    write_cmd(0x2B); write_data_word(uint16_t(y)); write_data_word(uint16_t(y2));
    write_cmd(0x2C);
    int sent = 0;
    while (sent < total) {
        int n = total - sent;
        if (n > 256) n = 256;
        write_data_buf(reinterpret_cast<const uint8_t *>(buf), n * 2);
        sent += n;
    }
    spi_device_release_bus(spi_dev);
}

void st7796_flush(int x1, int y1, int x2, int y2, const uint16_t *data)
{
    size_t len = size_t(x2 - x1 + 1) * (y2 - y1 + 1);
    size_t bytes = len * 2;

    spi_device_acquire_bus(spi_dev, portMAX_DELAY);
    write_cmd(0x2A);
    write_data_word(uint16_t(x1));
    write_data_word(uint16_t(x2));
    write_cmd(0x2B);
    write_data_word(uint16_t(y1));
    write_data_word(uint16_t(y2));
    write_cmd(0x2C);

    const uint8_t *ptr = reinterpret_cast<const uint8_t *>(data);
    gpio_set_level((gpio_num_t)PIN_LCD_DC, 1);
    size_t remaining = bytes;
    while (remaining > 0) {
        size_t n = remaining > 4096 ? 4096 : remaining;
        spi_transaction_t t{};
        t.length    = n * 8;
        t.rxlength  = 0;
        t.tx_buffer = ptr;
        spi_device_transmit(spi_dev, &t);
        ptr  += n;
        remaining -= n;
    }

    spi_device_release_bus(spi_dev);
    ESP_LOGD("lcd", "flush (%d,%d)-(%d,%d) %dpx", x1, y1, x2, y2, (int)len);
}
