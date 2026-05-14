# Touch & Display Notes

## 1. Display (ST7796S) Orientation

**MADCTL = 0xE8** in `st7796.cpp:71`

| Bit | Name | Value | Effect |
|-----|------|-------|--------|
| 7   | MY   | 1     | Row address order reversed (writes bottom→top within window) |
| 6   | MX   | 1     | Column address order reversed (writes right→left within window) |
| 5   | MV   | 1     | CASET→row(Y) address, RASET→column(X) address |
| 3   | BGR  | 1     | RGB→BGR swap |

Because MV=1, `st7796_fill_rect(x,y,w,h)` sends:
- CASET = x (→ row address on panel, 0-479 → top→bottom)
- RASET = y (→ column address on panel, 0-319 → left→right)

**Display logical origin (0,0) is at physical TOP-LEFT.**  
MY/MX only affect data write ORDER within the window, not address-to-pixel mapping.

## 2. Touch Sensor (FT6336U) Raw Coordinates

From empirical testing (user tapping corners):

| Physical corner | Raw touch (x, y) |
|----------------|-----------------|
| Top-right      | (0, 0)          |
| Bottom-right   | (320, 0)        |
| Top-left       | (0, ~320)       |
| Bottom-left    | (320, ~320)     |

So (from empirical 4-corner calibration):
- **raw_x**: 0 (top) → **~300-305** (bottom)
- **raw_y**: 0 (right) → **~460-465** (left)
- NOT 12-bit (4095) on either axis. The ADC range is ~305×465.

## 3. Touch → Display Mapping

```cpp
/* In main.cpp */
#define TOUCH_X_MAX  305   /* raw_x max (bottom edge) — measured ~301 */
#define TOUCH_Y_MAX  465   /* raw_y max (left edge)   — measured ~461 */

int x = (TOUCH_Y_MAX - raw_y) * 479 / TOUCH_Y_MAX;  // raw_y: right(0)→left(465) → display X: 479→0
int y = raw_x * 319 / TOUCH_X_MAX;                    // raw_x: top(0)→bottom(305) → display Y: 0→319
```

**Why swap (raw_y→x, raw_x→y):** touch sensor is physically aligned portrait-native on a landscape display. The touch sensor's X (short axis) measures the vertical direction (display Y), and Y (long axis) measures the horizontal direction (display X).

**Why raw_y mirror:** the touch sensor's Y=0 is at the RIGHT edge, but display X=0 is at the LEFT edge.

**raw_y divisor = 465, raw_x divisor = 305:** determined empirically by tapping each corner and reading raw values.

### Calibration results

| Touch raw | Display expected | Display actual |
|-----------|-----------------|----------------|
| (12, 33)  | (479, 0) top-right | (460, 12) ✓ |
| (301, 7)  | (479, 319) bottom-right | (460, 300) ✓ |
| (9, 461)  | (0, 0) top-left | (4, 9) ✓ |
| (292, 457) | (0, 319) bottom-left | (8, 300) ✓ |

Note: display coordinates near edge are offset by SQUARE_SIZE/2 due to clamping.

## 4. I2C / FT6336 Init Issues

**Symptom:** `E (2152) ft6336: touch chip not responding after 5 tries`

**Cause:** Reading resolution registers 0xA4-0xA7 after chip ID check (0xA8) corrupted the I2C bus state. The second `ft_read_regs()` call left the bus in an invalid state, causing subsequent chip ID retries to fail.

**Fix:** Do NOT read registers 0xA4-0xA7. The FT6336U on this board doesn't tolerate back-to-back register reads via this bit-bang I2C implementation.

**If chip still not detected:** power-cycle (reset button may not be enough), or check SDA=27, SCL=29, RST=28 pin continuity.

## 5. Constraints

- The `st7796_fill_rect` clamping assumes SQUARE_SIZE=20 and display 480×320.
- The touch resolution (~320) is inferred from corner taps. To read actual resolution from chip registers 0xA4-0xA7, see note 4 — it may crash the I2C bus.
- Log output format: `pt0 raw(raw_x,raw_y) -> disp(disp_x,disp_y)` where disp is the computed display coordinate for point 0.
