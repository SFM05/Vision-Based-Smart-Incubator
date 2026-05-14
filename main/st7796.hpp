#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

#define ST7796_LCD_W      480
#define ST7796_LCD_H      320

/* Landscape 480x320 */
#define LVGL_HOR_RES      480
#define LVGL_VER_RES      320

/* ===================================================================
 *  Pin assignments — WT01P4C5 (ESP32-P4)
 *
 *  ⚠  DISPLAY + TOUCH pins are FIXED by the 14-pin FPC connector
 *     on WT01P4C5 — do NOT change unless you bypass the FPC.
 *  ⚠  Camera aux signals (GPIO 38~41, 47) are FIXED by the MIPI-CSI
 *     FPC connector — do NOT change.
 *
 *  MIPI-CSI data/clock lanes are dedicated PHY pins on ESP32-P4
 *  and are routed to the FPC connector on WT01P4C5 — not regular GPIOs.
 * =================================================================== */

/* ---- Display (ST7796) — SPI2 ---- */
#define PIN_LCD_MOSI   32           /* IO32 */
#define PIN_LCD_MISO   30           /* IO30 */
#define PIN_LCD_SCLK   31           /* IO31 */
#define PIN_LCD_CS     36           /* IO36 */
#define PIN_LCD_DC     33           /* IO33 */
#define PIN_LCD_RST    34           /* IO34 */
#define PIN_LCD_LED    -1           /* tie to 3.3V if constant on */

/* ---- Touch (FT6336U) — I2C0 ---- */
#define PIN_TOUCH_SDA  27           /* IO27 */
#define PIN_TOUCH_SCL  29           /* IO29 */
#define PIN_TOUCH_INT  26           /* IO26 */
#define PIN_TOUCH_RST  28           /* IO28 */

/* ---- Camera (OV5647) — MIPI-CSI + I2C0 (SCCB) ---- */
/* MIPI CSI data/clock lanes are dedicated PHY pins on ESP32-P4
 * (hardwired on the FPC connector — NOT regular GPIOs).
 * Auxiliary camera signals below are from official WTDKP4C5-S1 BSP. */
#define PIN_CAM_SDA    7           /* SCCB data (I2C0) */
#define PIN_CAM_SCL    8           /* SCCB clock (I2C0) */
#define PIN_CAM_RST    41          /* Sensor reset */
#define PIN_CAM_PWDN   38          /* Power down */
#define PIN_CAM_MCLK   47          /* Master clock */

void st7796_init(void);
void st7796_fill_screen(uint16_t color);
void st7796_fill_rect(int x, int y, int w, int h, uint16_t color);
void st7796_flush(int x1, int y1, int x2, int y2, const uint16_t *data);

#ifdef __cplusplus
}
#endif
