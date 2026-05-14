#include "ft6336.hpp"
#include "st7796.hpp"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"

static const char *TAG = "ft6336";

#define PIN_SDA  ((gpio_num_t)PIN_TOUCH_SDA)
#define PIN_SCL  ((gpio_num_t)PIN_TOUCH_SCL)
#define PIN_RST  ((gpio_num_t)PIN_TOUCH_RST)

#define I2C_ADDR  0x38

static bool s_touch_present = false;

/* ================================================================ */
/*  Bit-bang I2C — mode-toggle approach (proven to work)           */
/* ================================================================ */

static void scl_low(void)  { gpio_set_level(PIN_SCL, 0); }
static void scl_high(void) { gpio_set_level(PIN_SCL, 1); }
static void sda_low(void)  { gpio_set_level(PIN_SDA, 0); }
static void sda_high(void) { gpio_set_level(PIN_SDA, 1); }
static int  sda_rd(void)   { return gpio_get_level(PIN_SDA); }
static void bdelay(void)   { ets_delay_us(10); }
static void short_dly(void) { ets_delay_us(2); }

static void sda_set_out(void) { gpio_set_direction(PIN_SDA, GPIO_MODE_OUTPUT_OD); }
static void sda_set_inp(void) { gpio_set_direction(PIN_SDA, GPIO_MODE_INPUT); }

static void i2c_init_pins(void)
{
    gpio_reset_pin(PIN_SCL);
    gpio_reset_pin(PIN_SDA);
    vTaskDelay(pdMS_TO_TICKS(5));

    gpio_set_direction(PIN_SCL, GPIO_MODE_OUTPUT_OD);
    scl_high();
    sda_set_out();
    sda_high();
    gpio_set_pull_mode(PIN_SCL, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_SDA, GPIO_PULLUP_ONLY);

    for (int i = 0; i < 9; i++) {
        scl_low(); bdelay();
        scl_high(); bdelay();
    }
    sda_set_out(); sda_low();
    scl_high(); bdelay();
    sda_high(); bdelay();
}

static void i2c_start(void)
{
    sda_set_out(); sda_high();
    scl_high(); bdelay();
    sda_low();  bdelay();
    scl_low();  bdelay();
}

static void i2c_stop(void)
{
    sda_set_out(); sda_low();
    scl_high(); bdelay();
    sda_high(); bdelay();
}

static int i2c_write(uint8_t data)
{
    sda_set_out();
    for (int i = 0; i < 8; i++) {
        if (data & 0x80) sda_high(); else sda_low();
        data <<= 1;
        bdelay();
        scl_high(); bdelay(); scl_low(); bdelay();
    }
    sda_set_inp(); short_dly();
    scl_high(); bdelay();
    int ack = sda_rd();
    scl_low(); bdelay();
    sda_set_out();
    return ack;
}

static uint8_t i2c_read(int send_nack)
{
    uint8_t data = 0;
    sda_set_inp();
    for (int i = 0; i < 8; i++) {
        data <<= 1;
        scl_high(); bdelay();
        if (sda_rd()) data |= 1;
        scl_low(); bdelay();
    }
    sda_set_out();
    if (send_nack) sda_high(); else sda_low();
    bdelay();
    scl_high(); bdelay(); scl_low(); bdelay();
    sda_set_inp();
    return data;
}

static int ft_read_regs(uint8_t reg, uint8_t *buf, int len)
{
    i2c_start();
    if (i2c_write(I2C_ADDR << 1)) { i2c_stop(); return -1; }
    if (i2c_write(reg))           { i2c_stop(); return -2; }
    i2c_start();
    if (i2c_write((I2C_ADDR << 1) | 1)) { i2c_stop(); return -3; }
    for (int i = 0; i < len; i++)
        buf[i] = i2c_read(i == len - 1);
    i2c_stop();
    return 0;
}

/* ================================================================ */
/*  Public API                                                       */
/* ================================================================ */

void ft6336_init(void)
{
    ESP_LOGI(TAG, "init SDA=%d SCL=%d RST=%d",
             PIN_TOUCH_SDA, PIN_TOUCH_SCL, PIN_TOUCH_RST);

    gpio_set_direction(PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(300));

    gpio_set_direction((gpio_num_t)PIN_TOUCH_INT, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)PIN_TOUCH_INT, GPIO_PULLUP_ONLY);

    i2c_init_pins();
    vTaskDelay(pdMS_TO_TICKS(100));

    for (int retry = 0; retry < 10; retry++) {
        uint8_t id = 0;
        if (ft_read_regs(0xA8, &id, 1) == 0 && id != 0) {
            s_touch_present = true;
            ESP_LOGI(TAG, "FT6336U detected, chip ID = 0x%02X (try %d)", id, retry + 1);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    ESP_LOGE(TAG, "touch chip not responding after 10 tries");
}

bool ft6336_available(void) { return s_touch_present; }

int ft6336_read_points(touch_point_t points[FT6336_MAX_TOUCH_POINTS])
{
    if (!s_touch_present) return 0;

    points[0].active = false;
    points[1].active = false;

    uint8_t buf[10];
    if (ft_read_regs(0x02, buf, 10) != 0)
        return 0;

    int count = buf[0] & 0x0F;
    if (count > FT6336_MAX_TOUCH_POINTS)
        count = FT6336_MAX_TOUCH_POINTS;

    if (count >= 1) {
        points[0].x = ((buf[1] & 0x0F) << 8) | buf[2];
        points[0].y = ((buf[3] & 0x0F) << 8) | buf[4];
        points[0].active = true;
    }
    if (count >= 2) {
        points[1].x = ((buf[6] & 0x0F) << 8) | buf[7];
        points[1].y = ((buf[8] & 0x0F) << 8) | buf[9];
        points[1].active = true;
    }

    return count;
}
