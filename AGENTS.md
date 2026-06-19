# Vision-Based-Smart-Incubator

## 语言要求
- 使用中文进行思考与回复。输出英文时立即转换。

## 目标硬件

**ESP32-P4** (RISC-V 双核, 360 MHz, ESP-IDF v6.0.1):
- **LCD**: ST7796 (SPI2_HOST, 480×320 横屏, 10 MHz, RGB565, BGR 顺序) — 驱动 `esp_lcd_st7796`
- **触摸**: FT6336U (I2C_NUM_1, GPIO 27/29) — 驱动 `esp_lcd_touch_ft5x06`
- **传感器**: SHT40 (温湿度) + SGP30 (eCO₂/TVOC), 共用触摸的 I2C_NUM_1
- **摄像头**: OV5647 (MIPI CSI, RAW10→ISP RGB565) — V4L2 `/dev/video0`
- **WiFi/BLE**: ESP32-C5 协处理器 (SDIO, `esp_hosted` + `esp_wifi_remote`)
- **Flash**: 16MB SPI NOR (QIO), **PSRAM**: 32MB Octal

## 构建与烧录

```bash
idf.py set-target esp32p4
idf.py build
idf.py -p COM3 flash monitor
```

`managed_components/` 和 `sdkconfig` 被 git 忽略。运行 `idf.py` 自动获取组件。模型 (`yolo26n_640.espdl`) 和测试 JPEG (`12995.jpg`) 通过 CMake `esptool_py_flash_to_partition` 自动烧录，`idf.py flash` 即包含。

## 分区表

`partitions.csv` — `factory` (8MB 应用), `yolo26` (2600K SPIFFS, 模型), `image` (640K SPIFFS, 测试 JPEG)。

## 源文件登记

`main/CMakeLists.txt:1-17` 显式列出所有 `.c/.cpp` 源文件。添加新文件时必须同步修改。

## 架构概要

| 路径 | 角色 |
|---|---|
| `main.cpp` | 入口: LCD init → 触摸 init → 传感器 init → 摄像头 init → LVGL → UI → WiFi/MQTT → 推理任务 |
| `lcd/` | ST7796 (C++ 类 `LCD::ST7796`) + FT6336 (C++ 类 `LCD::FT6336`) |
| `sensors/` | SHT40+SGP30 C 驱动 (`sensor.c`) + FreeRTOS 读取任务 (`sensor_task.cpp`, 1 Hz) |
| `lvgl_port/` | LVGL v8.4 初始化、flush 回调、触摸回调 |
| `inference_task/` | V4L2 帧捕获 → 裁剪 640×640 → YOLO 推理 → LVGL canvas 更新 |
| `yolo26/` | YOLO 推理辅助函数 (`decode_jpeg`, `resize`, `yolo_inference`) + `ImageAuto` RAII 包装 |
| `ui/` | LVGL GUI Guider 生成的 UI 代码 |
| `iot/` | WiFi (singleton)、MQTT (singleton)、NTP、HTTP 上传、NVS |

**C++ 文件** (`main.cpp`, `inference_task.cpp`, etc.) 通过 `extern "C"` 头文件调用 C 模块 (`sensor.c`)。

## 关键约定

### 初始化顺序 (main.cpp:140-154)
触摸 I2C → `init_sensor_devices(touch.i2c_handle())` → 触摸 IO init → 触摸 touch init。
**传感器设备加到触摸的 I2C 总线上之后才初始化触摸**。

### 摄像头延迟初始化 (inference_task.cpp:454-464)
先处理静态 JPEG 图片，**然后**才初始化摄像头。推理缓冲区提前分配会影响 ISP DMA 空间。

### DMA 缓冲区 — 必须遵守
- **LVGL draw buffer**: `MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA` (128 字节对齐, 2 × 75KB)
- **SPI DMA buffer**: `MALLOC_CAP_DMA` (不在 PSRAM)
- **大缓冲区**: `MALLOC_CAP_SPIRAM` (全帧、canvas、模型输入)

### PSRAM 缓存同步
PSRAM 缓冲区被 DMA 外设读取前必须：`esp_cache_msync(buf, size, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED)`

### 镜像配置同步
`panel.set_mirror(cfg)` 后必须立即调用 `LvglPort::set_mirror(cfg)`，否则触摸坐标错位。当前生产值为 `{mv=true, mx=false, my=false}` (MADCTL=0x28)。

### 两个 I2C 总线
- **I2C_NUM_1** (GPIO 27 SDA / 29 SCL, 100 kHz): 触摸 + SHT40(0x44) + SGP30(0x58)
- **I2C_NUM_0** (GPIO 7 SDA / 8 SCL): 摄像头 SCCB (OV5647, 0x36)

### 推理期间喂 Task WDT
YOLO 推理 (最多数秒) 期间必须 `esp_task_wdt_add(NULL)` → 期间 `esp_task_wdt_reset()` → 结束 `esp_task_wdt_delete(NULL)`。WDT 超时 30 秒。

### 推理时屏蔽触摸
`lvgl_port.cpp:43` — 推理运行时触摸读回调返回 `LV_INDEV_STATE_REL`，防止干扰。

### V4L2 摄像头
- `ESP_VIDEO_MIPI_CSI_DEVICE_NAME` 打开 `/dev/video0`
- `VIDIOC_REQBUFS` count=2, `V4L2_MEMORY_MMAP`
- ISP BF 去噪通过 `V4L2_CID_USER_ESP_ISP_BF` ext control 配置 (3×3 均值核, level=15)
- 帧到达后 `esp_cache_msync(DIR_M2C)` 确保 PSRAM coherence

### YOLO 模型
- 模型从 SPIFFS 分区 `"yolo26"` 加载: `new dl::Model("yolo26", fbs::MODEL_LOCATION_IN_FLASH_PARTITION)`
- 输入 640×640 RGB888, `YOLO26` 处理器 (top_k=60, conf=0.25, classes=coco_classes)
- 硬件 JPEG 编码用于上传: `dl::image::hw_encode_jpeg(img, 85)`

### WiFi/MQTT 配置
通过 `idf.py menuconfig` → "IOT Configuration" 设置 SSID/密码/MQTT 地址。Kconfig 符号: `CONFIG_IOT_WIFI_SSID/PASSWORD`, `CONFIG_IOT_MQTT_SERVER/USERNAME/PASSWORD`。

### 核心分配
- `capture_task_fn` (推理): core 1, 优先级 `configMAX_PRIORITIES - 2`
- `sensor_read_task_fn`: core 0, 优先级 5
- `upload_task`: 优先级 5
- `app_main` 优先级提升到 5

### sdkconfig.defaults 关键项
`QIO` 模式, `32MB` 闪存, `PSRAM=y`, `L2_CACHE_256KB`, `CACHE_LINE_128B`, `LV_COLOR_16_SWAP=y`, `ESP_TASK_WDT_INIT=n`, `ESP_INT_WDT_TIMEOUT_MS=800`, `ESP_MAIN_TASK_STACK_SIZE=8000`。

### `.clangd`
去除 `-f*` 和 `-m*` 编译标志。`compile_commands.json` 在 `build/` 中。

### 重置流程 (修改组件/依赖后)
```bash
idf.py fullclean        # 清理构建
rm sdkconfig            # 删除旧配置
idf.py reconfigure      # 重新生成
idf.py build
```
烧录后必须 **拔掉 USB 完全断电再上电**（仅按 EN 键 LCD 可能不完全复位）。

### Managed 组件补丁
`espressif__esp_ipa/CMakeLists.txt`, `espressif__esp_video/CMakeLists.txt`, `esp_video_isp_device.c`, `esp_video_csi_device.c` 有手工补丁。**`idf.py update-dependencies` 会覆盖这些修改**，需重新应用。

`espressif__esp_hosted` (v2.12.7) — `host/port/esp/freertos/src/port_esp_hosted_host_os.c:51-52` 中 `g_wifi_default_wpa_crypto_funcs` 和 `g_wifi_osi_funcs` 缺少 `extern`，导致 BSS 定义覆盖 `esp_adapter.c` 的 DATA 定义，WiFi 初始化时报 `net80211: OS adapter function version error`。详见 `docs/patches/esp_hosted_g_wifi_osi_funcs.md`。

## 单元测试

`main/test.cpp` 提供三个独立测试函数 (`lcd_test`, `yolo_test`, `iot_test`)，未接入 CI (无 .github 目录)。编译时需取消 `main.cpp` 中 `app_main` 注释并将测试函数名改为 `app_main`。

## MCP 工具用量限制

`espressif-documentation_search_espressif_sources` 每日 200 次 / 每小时 60 次。仅在必要时调用。
