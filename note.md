# 开发笔记

## 屏幕配置
- 物理面板: ST7796, 480×320 (横向), SPI 接口
- 驱动: esp_lcd_st7796 v1.4.0 (general SPI mode)
- SPI 时钟: 10MHz
- 物理分辨率 480×320，但 ST7796 默认坐标是 320×480（纵向）

### 方向变换 — LVGL 模式下禁用 swap_xy
**LVGL 条带刷新时不能使用 swap_xy**。原因是 swap_xy 交换了行列坐标，但 LVGL 的像素数据是按 LVGL 逻辑行列排列的，与物理 GRAM 的填充方向不匹配，导致画面割裂/花屏。

解决方法：LVGL 直接设置为物理分辨率 480×320（横屏），不调用 `swap_xy/mirror`。

纯色测试中可以使用 swap_xy（全屏单色时数据重复无影响），但 LVGL 条带刷新时必须禁用。

### 颜色字节序
ST7796 期望 SPI 收到**大端(BE)**字节序，但 ESP32-P4 CPU 是**小端(LE)**。

RGB565 像素存储:
- LE (CPU 原生): `{G[2:0]|B[4:0], R[4:0]|G[5:3]}` — 低字节在前
- BE (ST7796 期望): `{R[4:0]|G[5:3], G[2:0]|B[4:0]}` — 高字节在前

手动转换：
```c
uint16_t rgb565_be(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t le = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    return (le >> 8) | (le << 8);
}
```

### 红蓝反色
此 ST7796 模块硬件连线为 BGR 顺序，需要：
```cpp
m_panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
```
若使用 `LCD_RGB_ELEMENT_ORDER_RGB`，红蓝通道互换。

## SPI 传输注意事项

### DMA 缓冲区
ST7796 SPI 驱动通过 DMA 发送像素数据，DMA buffer **必须从 `MALLOC_CAP_DMA` 分配**：
```cpp
// 正确：DMA 可访问
buf = heap_caps_malloc(size, MALLOC_CAP_DMA);

// 错误：SPI DMA 无法正常访问 SPIRAM，导致顶部花屏
buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
```

### 传输大小
`max_transfer_sz` 决定单次 SPI DMA 传输的最大字节数。SPI 驱动会将大传输自动拆包，但
拆包时 CS 可能拉高，导致 Display GRAM 指针偏移。建议单次传输覆盖一个完整条带：
```cpp
// 每条带 80 行，共 4 条带覆盖全屏
constexpr int BAND_H = 80;
panel.set_spi_cfg(mosi, clk, miso, 480 * BAND_H * sizeof(uint16_t));
```
LVGL 也采用条带刷新 (320×120 像素/条带)。

## LVGL 配置

### sdkconfig.defaults 关键选项
```ini
# 字节序：ST7796 需要 BE，CPU 是 LE
CONFIG_LV_COLOR_16_SWAP=y

# 禁用中断看门狗（ESP32-P4 双核，CPU1 空闲时不处理中断）
CONFIG_ESP_INT_WDT=n

# 禁用 Task WDT 启动时初始化（初始化耗时可能超过 5s）
CONFIG_ESP_TASK_WDT_INIT=n
```

### 花屏问题根因汇总
| 现象 | 原因 | 修复 |
|------|------|------|
| 顶部花屏，下方正方形正确 | 缺少 swap_xy，坐标不匹配 | 加 `swap_xy(true)` |
| 顶部花屏，其余正确 | SPI DMA buffer 在 SPIRAM 中 | 改用 `MALLOC_CAP_DMA` |
| 全屏绿色块/花屏 | PSRAM 未初始化或字节序错误 | `memset(buf,0,...)` + `LV_COLOR_16_SWAP=y` |
| 红蓝反色 | rgb_ele_order 应为 BGR | `LCD_RGB_ELEMENT_ORDER_BGR` |
| 绿蓝反色 | 未做 LE→BE 字节交换 | `rgb565_be()` 函数或 `LV_COLOR_16_SWAP=y` |
| 白屏，串口停在 Started on CPU0 | Task WDT 5s 超时复位 | `CONFIG_ESP_TASK_WDT_INIT=n` |
| Guru Meditation (Int WDT) | CPU1 空闲时中断看门狗超时 | `CONFIG_ESP_INT_WDT=n` |
| App 无法启动（无日志） | sdkconfig 重新生成后默认配置不兼容 | 删除 sdkconfig 重新配置 |

## GPIO 引脚 (WIRING.md)
### LCD (ST7796) — SPI
| 信号 | GPIO |
|------|------|
| CS | 36 |
| RST | 34 |
| RS/DC | 33 |
| MOSI | 32 |
| MISO | 30 |
| CLK | 31 |

### 触摸 (FT6336) — I2C
| 信号 | GPIO |
|------|------|
| SCL | 29 |
| RST | 28 |
| SDA | 27 |
| INT | 26 |

### 触控标定
FT6336U 物理 portrait（320×480），屏幕逻辑 landscape（480×320）。
标定值见 `TOUCH_DISPLAY_NOTES.md`：raw_x=305, raw_y=465。
映射：raw_x(0~305)→disp_y(0~319)，raw_y(465~0)→disp_x(0~479)。

### 摄像头 (OV5647) — MIPI CSI
| 信号 | GPIO | 说明 |
|------|------|------|
| SDA | **7** | I2C_NUM_0 (与触摸分属不同端口) |
| SCL | **8** | I2C_NUM_0 |
| PWDN | N/A | CSI 硬件管理，软件不控制 |
| RST | N/A | CSI 硬件管理，软件不控制 |
| MCLK | N/A | CSI 硬件提供时钟，无独立 GPIO |

按官方 `esp-idf/examples/peripherals/camera/mipi_isp_dsi` 示例配置：
- I2C: I2C_NUM_0, GPIO 7(SDA)/8(SCL)
- 触摸使用 I2C_NUM_1, GPIO 27(SDA)/29(SCL)
- PWDN/RST/MCLK 全部由 CSI 硬件管理，软件传 -1

### 摄像头 I2C 地址
OV5647 的 7-bit I2C 地址 = **0x36**（官方 `esp_cam_sensor` 驱动值）。
I2C 诊断在摄像头探测失败时自动运行，会扫描地址 0x08-0x77 并报告 SDA/SCL 电平状态。

## 视频管道 (V4L2)
摄像头画面采集依赖 `espressif/esp_video`（注册中心组件），通过 V4L2 接口从 `/dev/video0` 获取帧数据：
- 帧采集 → 最近邻缩放 → 写入画布缓冲区
- 支持 framebuffer 模式（写入 LVGL canvas）和 LCD 直写模式
- busy flag 同步：预览任务在 LVGL 渲染时跳过帧写入，防止撕裂

## LVGL Canvas 画布
- 位置：屏幕右侧 (x=70, y=0)，尺寸 410×320
- 缓冲区：从 PSRAM 分配（262,400 bytes），创建时填充测试彩条
- 无摄像头时显示测试彩条（红/绿/蓝/白/黑），摄像头工作后显示实时预览
- 画布不可点击 (`lv_obj_clear_flag LV_OBJ_FLAG_CLICKABLE`)

## 显示问题排查
### 白屏 / 画面异常
| 现象 | 可能原因 | 检查项 |
|------|---------|--------|
| 画布彩条正常 | 显示通路 OK | 摄像头硬件 |
| 全屏白/无显示 | LVGL draw buffer 在 PSRAM | `MALLOC_CAP_DMA` 分配 |
| 画布黑色 | canvas buffer 全零 | 摄像头无数据 |
| 颜色错误 | 字节序/BGR 顺序 | `LV_COLOR_16_SWAP`, `rgb_ele_order` |

### 烧录后白屏修复记录 (2026-05-21)

**现象**：烧录后屏幕全白，LCD 测试（4 色彩条）正常，LVGL 初始化后白屏。

**根因**：LVGL draw buffer 不是从 DMA 内存分配的。SPI DMA 无法访问 PSRAM，当 buffer 回退到 PSRAM 时屏幕全白。

**涉及文件**：
- `main/lvgl_port/lvgl_port.cpp`：去除 SPIRAM 回退，DMA 分配失败直接报错
- `main/lcd/st7796_driver.cpp`：面板初始化后直接发送 MADCTL=0xE8，避免调用 `swap_xy()`
- `main/main.cpp`：去掉 `esp_lcd_panel_swap_xy()` 和 `esp_lcd_panel_mirror()`

**修复步骤**：
1. 修改 `lvgl_port.cpp`：`MALLOC_CAP_SPIRAM` 回退 → `MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA`
2. 去掉 `main.cpp` 中的 `swap_xy()`/`mirror()`，在 `st7796_driver.cpp` 的 `panel_init()` 中通过 `esp_lcd_panel_io_tx_param()` 直接设 MADCTL=0xE8
3. 增大 DMA 保留池：`sdkconfig.defaults` 加 `CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL=196608`
4. **必须执行**：`idf.py fullclean` → 删除 `sdkconfig` → `idf.py reconfigure` → `idf.py build`
5. **烧录后必须拔掉 USB 完全断电再重新上电**（仅按 EN 键 LCD 可能不完全复位）

## 触摸 I2C 问题

### FT6336U I2C 间歇性失败
**日志**：
```
E (27353) lcd_panel.io.i2c: panel_io_i2c_rx_buffer(145): i2c transaction failed
E (27353) FT5x06: esp_lcd_touch_ft5x06_read_data(186): I2C read error!
```

**原因**：FT6336U 的 I2C 总线偶尔进入卡死状态（硬件特性），表现为偶发性的 NACK/超时。**非软件 bug**。

**处理**：
- `lvgl_port.cpp` 触摸回调中加入立即重试（`esp_lcd_touch_read_data()` 失败后重试一次）
- 去掉 `main.cpp` 中原有的每 30 秒盲复位（复位脉宽 10ms 不够，且不复位也能正常工作）

**若频繁出现**：
1. 检查 SDA(GPIO27)/SCL(GPIO29) 是否接了外部 4.7kΩ 上拉电阻（内部弱上拉可能不足）
2. 尝试降低 I2C 时钟：`m_io_cfg.scl_speed_hz = 100000` → `50000`
3. 确保 CTP_RST(GPIO28) 不被其他外设干扰
