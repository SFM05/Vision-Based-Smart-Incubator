# 配置参数记录 (2026-05-25)

## ST7796 LCD (SPI)

| 参数 | 值 | 说明 |
|------|----|------|
| SPI 时钟 | 10 MHz | |
| SPI 模式 | 0 | CPOL=0, CPHA=0 |
| SPI 主机 | SPI2_HOST | |
| max_transfer_sz | 0 (auto) | |
| trans_queue_depth | 10 | |
| RGB 元素顺序 | LCD_RGB_ELEMENT_ORDER_BGR | |
| BPP | 16 | RGB565 |
| MADCTL | 0xE8 (驱动API设置) | MV=1(横屏) + MX=1 + MY=1 + BGR=1 |
| 设置方式 | `swap_xy(1)` + `mirror(1,1)` | 驱动API，非直接写寄存器 |
| 全帧缓冲区 | `MALLOC_CAP_SPIRAM` | PSRAM, 经 `esp_cache_msync(C2M)` |
| 绘制 | `draw_bitmap(panel, 0, 0, 320, 480, buf)` | GRAM portrait 320×480 |
| 缓冲区不释放 | static 指针 | |

### 已验证的颜色映射 (BGR顺序, 通道循环置换 R→B, G→R, B→G)

| 发送 pixel | 显示颜色 |
|-----------|---------|
| `0x07E0` | 红 |
| `0x001F` | 绿 |
| `0xF800` | 蓝 |
| `0xFFFF` | 白 |
| `0x0000` | 黑 |

### 字节序
- CPU 小端 (LE), SPI 发送 = 内存字节顺序
- ST7796 SPI 接收: **第一字节 = 高字节** (大端 SPI)
- 不需要字节交换, 直接用 LE uint16_t

## FT6336 触摸 (I2C)

| 参数 | 值 | 说明 |
|------|----|------|
| I2C 时钟 | 100 kHz | |
| I2C 地址 | 0x38 | |
| I2C 端口 | -1 (自动) | |
| disable_control_phase | 1 | |
| touch flags | swap_xy=0, mirror_x=0, mirror_y=0 | 驱动不做任何变换 |
| x_max / y_max | 480 / 320 | 匹配显示分辨率 |

### 触控→显示映射 (最终确认, 2026-05-25)

```
raw_x = pt.x (垂直轴, 0=上 ~ 319=下)
raw_y = pt.y (水平轴, 0=右 ~ 477=左)
```

**映射公式:**
```c
disp_x = (477 - raw_y) * 479 / 477;   // raw_y(右→左) → disp_x(479→0)
disp_y = raw_x;                        // raw_x(上→下) → disp_y(0→319)
```

**显示旋转 (MX=1, MY=1) 补偿映射:**
```c
// 物理触摸 → LVGL 坐标 (含 180° 旋转补偿)
lvgl_x = disp_x;                       // 由 raw_y 决定
lvgl_y = disp_y;                       // 由 raw_x 决定
// 注: disp_x/lvgl_x 天然对齐, 因 MX/MY 已修正显示方向
```

### 实测标定

| 物理位置 | raw(x,y) | 映射后 (X,Y) |
|---------|----------|-------------|
| 左上 | (0, 459) | (~0, 0) |
| 右上 | (0, 22) | (~479, 0) |
| 左下 | (319, 477) | (~0, 319) |
| 右下 | (319, 19) | (~479, 319) |

### I2C 注意事项
- FT6336U 的 I2C 总线偶尔硬件卡死, 软件已加重试

## LVGL 配置

| 参数 | 值 |
|------|----|
| 分辨率 | 480×320 (横屏) |
| Draw buffer | 2 × 75KB, `MALLOC_CAP_INTERNAL \| MALLOC_CAP_DMA` |
| Flush | row-by-row `draw_bitmap(panel, x1, y, x2+1, y+1, row)` |
| 触控回调 | 直接使用 `lvgl_touch_read_cb` (无 skip) |

### LVGL 触控回调映射
```cpp
// lvgl_port.cpp lvgl_touch_read_cb:
int rx = point.x;  // raw_x 0-319
int ry = point.y;  // raw_y 0-477
data->point.x = (477 - ry) * 479 / 477;
data->point.y = rx;
```

## sdkconfig.defaults 关键项
- `CONFIG_LV_COLOR_16_SWAP=y`
- `CONFIG_LV_FONT_MONTSERRAT_20=y`
- `CONFIG_LV_FONT_MONTSERRAT_24=y`
- `CONFIG_ESP_TASK_WDT_INIT=n`
- `CONFIG_CACHE_L2_CACHE_256KB=y`
- `CONFIG_CACHE_L2_CACHE_LINE_128B=y`
