# 硬件接线说明 — WT01P4C5 + MSP4031 (ST7796 + FT6336U)

## 屏幕模块引脚 → WT01P4C5 (ESP32-P4)

| 序号 | 模块引脚     | 功能                     | WT01P4C5 FPC 硬连接 |
|------|-------------|--------------------------|--------------------|
| 1    | VCC         | 屏电源正 (5V 推荐)        | 5V                 |
| 2    | GND         | 屏电源地                  | GND                |
| 3    | **LCD_CS**  | SPI 片选                  | **IO36** (GPIO 36) |
| 4    | **LCD_RST** | 液晶复位 (低电平复位)      | **IO34** (GPIO 34) |
| 5    | **LCD_RS**  | 数据/命令选择             | **IO33** (GPIO 33) |
| 6    | **SDI**     | SPI MOSI                  | **IO32** (GPIO 32) |
| 7    | **SCK**     | SPI 时钟                  | **IO31** (GPIO 31) |
| 8    | LED         | 背光控制                  | **3.3V** (常亮)    |
| 9    | **SDO**     | SPI MISO                  | **IO30** (GPIO 30) |
| 10   | **CTP_SCL** | 触摸 I2C 时钟             | **IO29** (GPIO 29) |
| 11   | **CTP_RST** | 触摸复位 (低电平复位)      | **IO28** (GPIO 28) |
| 12   | **CTP_SDA** | 触摸 I2C 数据             | **IO27** (GPIO 27) |
| 13   | **CTP_INT** | 触摸中断 (触摸时低电平)    | **IO26** (GPIO 26) |
| 14   | SD_CS       | SD 卡 SPI 片选 (未使用)   | 悬空 / NC          |

## 代码对应的引脚定义

文件：`main/st7796.hpp`

```cpp
#define PIN_LCD_MOSI   32      // IO32
#define PIN_LCD_MISO   30      // IO30
#define PIN_LCD_SCLK   31      // IO31
#define PIN_LCD_CS     36      // IO36
#define PIN_LCD_DC     33      // IO33
#define PIN_LCD_RST    34      // IO34
#define PIN_LCD_LED    -1      // 3.3V 常亮

#define PIN_TOUCH_SDA  27      // IO27
#define PIN_TOUCH_SCL  29      // IO29
#define PIN_TOUCH_INT  26      // IO26
#define PIN_TOUCH_RST  28      // IO28

// 摄像头引脚（按官方 mipi_isp_dsi 示例）
#define PIN_CAM_SDA    7
#define PIN_CAM_SCL    8
```

