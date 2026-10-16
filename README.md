# Vision-Based Smart Incubator（基于视觉的智能培养箱）

基于 **ESP32-P4** 的智能培养箱控制系统。设备通过 MIPI-CSI 摄像头周期性拍摄培养皿，使用 **YOLO26** 神经网络检测并统计菌落数量，再通过手工特征 + 线性分类器识别**杂菌（污染）**，同时采集温湿度环境数据，最终在 LCD 上实时显示结果、在 SD 卡本地存档，并通过以太网把图像与数据上报到云端服务器。

> 详细开发约定见仓库根目录的 `AGENTS.md`（本地文件，不纳入版本控制）。

---

## 目录

- [1. 项目简介](#1-项目简介)
- [2. 功能特性](#2-功能特性)
- [3. 系统架构](#3-系统架构)
- [4. 硬件平台与引脚](#4-硬件平台与引脚)
- [5. 目录结构](#5-目录结构)
- [6. 核心任务与数据流](#6-核心任务与数据流)
- [7. AI 菌落检测](#7-ai-菌落检测)
- [8. 杂菌检测算法](#8-杂菌检测算法)
- [9. 网络与云端通信](#9-网络与云端通信)
- [10. 用户界面](#10-用户界面)
- [11. 构建与烧录](#11-构建与烧录)
- [12. 分区表](#12-分区表)
- [13. 开发注意事项](#13-开发注意事项)
- [14. 依赖组件](#14-依赖组件)

---

## 1. 项目简介

| 项目 | 说明 |
|------|------|
| 项目名称 | Vision-Based Smart Incubator |
| 目标芯片 | ESP32-P4 |
| 开发框架 | ESP-IDF **v6.0.1** |
| 编译器 | esp-clang（C++17） |
| 图形库 | LVGL v8.4.0（GUI Guider 生成界面） |
| AI 框架 | esp-dl 3.3.6 + yolo26 组件 |
| 摄像头 | OV5647（MIPI-CSI，1920×1080 RAW10） |
| 网络 | 以太网（IP101 PHY），非 Wi-Fi |

设备工作流程：

```
摄像头采集 → 裁剪 → YOLO26 菌落检测 → 杂菌判定 → LCD 显示 + SD 卡存档 + MQTT/HTTP 上云
```

---

## 2. 功能特性

- **菌落自动计数**：YOLO26 目标检测，类别为 `Colony`，单帧最多保留 128 个目标。
- **杂菌（污染）识别**：基于颜色矩、颜色直方图与几何形状的 23 维手工特征，配合 27 维线性分类器在线判定，发现异常即通过 MQTT 告警。
- **环境监测**：SHT3x 串口温湿度传感器周期采集，可配置上报间隔。
- **实时可视化**：LVGL 双界面，实时显示检测画面（含检测框）、菌落计数、温湿度与污染指示灯。
- **本地存档**：检测原图以 BMP 格式、检测框以 TXT 格式写入 SD 卡，按 `盘号/时间戳` 组织目录。
- **云端上云**：MQTT 与服务器双向通信，采用预签名 URL + HTTP PUT 上传图像文件；支持从服务器同步时间。
- **可配置节拍**：通过 UI 下拉框动态调整采集间隔与上报间隔。

---

## 3. 系统架构

### 3.1 软件分层

```
┌──────────────────────────────────────────────────────────────┐
│                     应用层 (main/)                              │
│  main.cpp  任务创建与调度   frontend.cpp  显示/UI   backend.cpp 采集/推理/上传 │
├──────────────────────────────────────────────────────────────┤
│  业务模块                                                        │
│  yolo26/ 模型推理    abnormal_detect/ 杂菌检测   tcd/ 温控(可选) │
├──────────────────────────────────────────────────────────────┤
│  驱动/服务层                                                     │
│  lcd/ ST7796+FT6336   cam/ V4L2摄像头   sensors/ SHT3x           │
│  sdcard/ FATFS        iot/ 以太网+MQTT+HTTP+NTP                  │
├──────────────────────────────────────────────────────────────┤
│  ESP-IDF v6.0.1 + esp-dl + LVGL + esp_video                     │
└──────────────────────────────────────────────────────────────┘
```

### 3.2 运行时任务流水线

```
                      capture_inference_queue (深度2)
  ┌────────────┐  ──────────────────────────────►  ┌────────────┐
  │  Capture   │                                   │ Inference  │
  │  任务      │  ◄──────── inference_end ──────── │  任务      │
  └─────┬──────┘                                   └─────┬──────┘
        │ request_upload (深度2)                          │ start_detect
        ▼                                                ▼
  ┌──────────────┐   device/{MAC}/upload          ┌─────────────────┐
  │Upload Request│  ─────────►  MQTT 服务器        │  Detect Abnormal│
  └──────────────┘                                   └────┬────────┬───┘
        ▲                                             status│        │
        │ upload_feed_back_queue (深度5)                    ▼        │(污染)
        │ (服务器回包: 预签名URL)                     ┌────────────┐  └► MQTT warn
        │                                            │Show Abnormal│
  ┌──────────┐   HTTP PUT 预签名URL                  └────────────┘
  │  Upload  │ ─────────► 对象存储
  └──────────┘
                 flush_screen                     result
  ┌───────────────┐◄──────────────   ┌──────────────────┐
  │ Image Display │                  │  Env Display      │
  │  (canvas 画框)│                  │  (温湿度标签)      │
  └───────────────┘                  └──────────────────┘
```

- 生产者/消费者通过 **队列**（`xQueue*`）传递数据，通过 **信号量**（`xSemaphore*`）与 **互斥锁** 完成节拍同步与共享缓冲区保护。
- 主循环运行 LVGL 心跳 `lv_timer_handler()`（每 15 ms），所有 UI 修改均需 `lv_lock()/lv_unlock()`。

---

## 4. 硬件平台与引脚

### 4.1 外设清单

| 外设 | 型号 | 接口 | 说明 |
|------|------|------|------|
| 主控 | ESP32-P4 | — | 8 MB Flash 应用分区、32 MB Flash |
| 摄像头 | OV5647 | MIPI-CSI + SCCB(I2C) | 1920×1080 RAW10 @30fps |
| 显示屏 | ST7796 | SPI (SPI3_HOST) | 480×320 |
| 触摸 | FT6336 | I2C | 电容触摸，兼容 ft5x06 驱动 |
| 温湿度 | SHT3x | UART1 | 串口输出，非 I2C |
| 以太网 | IP101 | RMII + MDC/MDIO | 复位引脚独立 |
| 存储 | SD 卡 | SPI 模式 | 走 LDO VO4 供 3.3V |
| 状态指示 | LED | GPIO | 采集指示 |

### 4.2 引脚分配

**显示屏 ST7796（SPI3_HOST）**

| 信号 | GPIO |
|------|------|
| MOSI | 16 |
| CLK | 15 |
| CS | 54 |
| RS / DC | 18 |
| RST | 19 |

**触摸 FT6336（I2C）**

| 信号 | GPIO |
|------|------|
| SDA | 5 |
| SCL | 14 |
| RST | 6 |
| INT | 4 |

**摄像头 OV5647 SCCB（I2C_NUM_0）**

| 信号 | GPIO |
|------|------|
| SDA | 7 |
| SCL | 8 |

**温湿度 SHT3x（UART1）**

| 信号 | GPIO |
|------|------|
| TX | 2 |
| RX | 3 |

**其他**

| 功能 | GPIO | 备注 |
|------|------|------|
| 采集指示 LED | 23 | `backend.cpp` |
| 以太网 PHY 复位 | 51 | `iot/eth.cpp` |
| 加热片 PWM（温控） | 22 | 当前在 `main.cpp` 中注释，未启用 |
| SD 卡 MOSI/CMD | 44 | SPI 模式 |
| SD 卡 MISO/D0 | 39 | SPI 模式 |
| SD 卡 CLK | 43 | SPI 模式 |
| SD 卡 CS/D3 | 42 | SPI 模式 |

---

## 5. 目录结构

```
Vision-Based-Smart-Incubator/
├── CMakeLists.txt            # 顶层工程定义（project: Vision-Based-Smart-Incubator）
├── partitions.csv            # 自定义分区表
├── sdkconfig.defaults        # 默认 Kconfig 配置
├── main/                     # 唯一应用组件
│   ├── main.cpp              # app_main：硬件初始化 + 任务创建
│   ├── frontend.cpp/.hpp     # LVGL 初始化、显示刷新、UI 更新任务
│   ├── backend.cpp/.hpp      # 采集/推理/上传/环境上报任务
│   ├── Kconfig.projbuild     # IOT 与 SD 卡配置菜单
│   ├── idf_component.yml     # 组件依赖声明
│   ├── CMakeLists.txt        # 源文件清单、模型/图片烧录规则
│   ├── cam/                  # V4L2 摄像头驱动（camera 单例）
│   ├── lcd/                  # ST7796（SPI 屏）+ FT6336（I2C 触摸）驱动
│   ├── sensors/              # SHT3x（UART）驱动；sensor.h/c 为 SHT40+SGP30 I2C 备选驱动
│   ├── abnormal_detect/      # 杂菌检测算法 + 分类器权重（clf.h）
│   ├── yolo26/               # BMP 保存工具 + 模型文件(yolo26n_640.espdl) + 测试图
│   ├── tcd/                  # 温度控制（PWM 加热 + PID），默认未接入
│   ├── iot/                  # 以太网、MQTT、HTTP 上传、NTP、NVS
│   ├── sdcard/               # FATFS SD 卡挂载（单例）
│   └── ui/                   # GUI Guider 生成代码（勿手改）+ custom.cpp 回调
└── managed_components/       # 组件管理器下载的 19 个依赖组件
```

> ⚠️ `main/ui/` 下除 `custom.cpp/.hpp` 外的文件由 GUI Guider 自动生成，**不要手动修改**。

---

## 6. 核心任务与数据流

`app_main()`（`main/main.cpp`）完成初始化后创建以下 FreeRTOS 任务：

| 任务名 | 入口函数 | 栈 | 优先级 | 职责 |
|--------|----------|----|--------|------|
| `Capture` | `capture_process` | 8192 | 1 | 抓帧、裁剪为 1080×1080、写 SD、触发推理 |
| `Inference` | `inference_process` | 4096 | 1 | YOLO26 预处理/推理/后处理、结果写 SD |
| `Upload Request` | `upload_request_process` | 8192 | 1 | 组装 JSON 并向 MQTT 请求上传 |
| `Upload` | `upload_process` | 8192 | 1 | 收到服务器预签名 URL 后 HTTP PUT 上传文件 |
| `Detect Abnormal` | `detect_abnormal_process` | 8192 | 1 | 提取特征、杂菌判定、异常时 MQTT 告警 |
| `ReporEnv` | `report_env_process` | 6144 | 1 | 周期读取温湿度并上报 |
| `Image Display` | `image_display_process` | 5120 | 1 | 缩放画面、绘制检测框到 canvas |
| `Env Display` | `env_display_process` | 5120 | 1 | 刷新温湿度标签 |
| `Show Abnormal` | `show_abnormal_process` | 1024 | 1 | 根据结果点亮红/绿指示灯 |
| `UART Receive` | `SHT3x::uart_event_task` | 1024 | 2 | 解析 SHT3x 串口数据 |

主要共享资源（均在 `main.cpp` 创建）：

| 资源 | 类型 | 容量 | 用途 |
|------|------|------|------|
| `capture_inference_queue` | Queue | 2 × `frame_meta_data` | 采集 → 推理 |
| `capture_request_queue` | Queue | 2 × `frame_meta_data` | 采集 → 上传请求 |
| `upload_feed_back_queue` | Queue | 5 × `char[512]` | MQTT 事件 → 上传 |
| `status` | Queue | 2 × `bool` | 杂菌判定 → 指示灯 |
| `capture_inference_mutex` | Mutex | — | 保护 `cropped_buf` 共享缓冲 |
| `inference_end` | Semaphore | — | 推理/检测完成通知采集 |
| `start_detect` | Semaphore | — | 触发杂菌检测 |
| `flush_screen` | Semaphore | — | 触发画面刷新 |

**单帧完整链路**：

1. `Capture` 从 V4L2 取帧，裁剪区域 `{420, 0, 1499, 1079}` 得到 1080×1080 RGB565LE 的 `cropped_buf`。
2. 通过 `capture_inference_queue` 通知 `Inference`，同时将原图写成 BMP 存 SD 卡（`/sdcard/{plate_id}/{时间戳}.bmp`）。
3. `Inference` 对 `cropped_buf` 做 letterbox + SIMD 量化预处理，调用 `model->run()`，后处理得到检测框集合，写入 TXT（`/sdcard/{plate_id}/{时间戳}.txt`）。
4. 释放 `start_detect` 触发 `Detect Abnormal` 做杂菌判定；判定完成后释放 `inference_end`。
5. `Capture` 收到 `inference_end` 后，向 `capture_request_queue` 投递帧信息并释放 `flush_screen`。
6. `Upload Request` 发送 MQTT 上传请求；`Image Display` 刷新画面。
7. 服务器处理请求后回发预签名 URL，MQTT 事件回调入队 `upload_feed_back_queue`，`Upload` 从 SD 卡读取文件并 HTTP PUT 上传。

帧元数据 `frame_meta_data`：

```cpp
struct frame_meta_data {
    char time[20];    // "YYYYMMDD-HHMMSS"
    uint8_t plate_id; // 培养皿编号
};
```

---

## 7. AI 菌落检测

- **模型**：`main/yolo26/yolo26n_640.espdl`，YOLO26n、640×640 输入。
- **加载方式**：编译期烧录到 `yolo26` flash 分区，运行时通过 `dl::Model("yolo26", fbs::MODEL_LOCATION_IN_FLASH_PARTITION)` 直接从分区映射，**不经过文件系统**。
- **检测参数**（`main.cpp`）：
  - 类别：`{"Colony"}`
  - `TARGET_K = 128`（单帧最大目标数）
  - `CONF_THRESH = 0.25f`
- **推理流程**（`YOLO26` 类，来自 `espressif/yolo26` 组件）：
  1. `preprocess()`：esp-dl `ImagePreprocessor` 完成 letterbox 缩放与 SIMD 量化，直接写入模型 RAM。
  2. `model->run()`：执行推理。
  3. `postprocess()`：在 strides `{8, 16, 32}` 的多尺度网格上解码，整数阈值快速过滤，输出 `dl::detect::result_t` 列表（含 `box[4]` 与 `score`）。
- **结果用途**：计数显示、canvas 画框、杂菌检测输入、SD 卡记录、云端上报。

---

## 8. 杂菌检测算法

杂菌检测不依赖神经网络，而是"手工特征 + 线性分类器"，位于 `main/abnormal_detect/`。

### 8.1 特征提取（`extract_base_features`）

对每个检测框区域计算 **23 维基础特征**：

| 维度 | 特征 |
|------|------|
| 0–7 | 颜色矩：R/G/B 均值、标准差（6）+ 灰度均值、标准差（2） |
| 8–19 | 颜色直方图：R/G/B 各 4 个 bin（每 bin 右移 6 位分箱，归一化） |
| 20–22 | 几何：面积占比、长宽比、紧凑度 |

### 8.2 组合特征（`inference`）

- 计算所有目标的 **均值特征**（`extract_mean_feature`）。
- 对每个目标构造 **27 维输入**：23 维"与均值差的平方" + 4 维上下文特征（特征偏差范数、相似度比例、最近邻距离、局部密度）。
- 线性打分：`score = clf_bias + Σ(wᵢ · xᵢ)`，阈值 `CLF_THRESHOLD_F32 = 2.386294`。
- `score >` 阈值判定为污染，权重与偏置硬编码于 `clf.h`。

发现污染时向 MQTT 发送告警（主题 `device/{MAC}/warn`，载荷 `"competitor"`），并点亮 UI 红色指示灯。

---

## 9. 网络与云端通信

网络层基于 **以太网**（IP101 PHY，RMII，复位 GPIO51），Wi-Fi 相关 Kconfig 为遗留配置，未实际使用。

### 9.1 MQTT 主题

| 方向 | 主题 | 载荷 |
|------|------|------|
| 发布 | `device/{MAC}/upload` | 上传请求 JSON |
| 发布 | `device/{MAC}/data` | 环境数据 JSON |
| 发布 | `device/{MAC}/warn` | 污染告警 `"competitor"` |
| 发布 | `device/{MAC}/time` | 空载荷（触发服务器回发时间） |
| 订阅 | `server/{MAC}/#` | 服务器下发 |

`{MAC}` 为设备 MAC 十六进制字符串（`IOT::get_mac()`）。MQTT 证书使用 `esp_crt_bundle`。

### 9.2 消息格式

**上传请求（设备 → 服务器）**

```json
{
  "timestamp": "20260101-120000",
  "plateid": 0,
  "imgpath": "/sdcard/0/20260101-120000.bmp",
  "txtpath": "/sdcard/0/20260101-120000.txt",
  "number": 12
}
```

**服务器回包（服务器 → 设备，`server/{MAC}/upload`）**

```json
{ "success": true, "url": "https://...预签名URL...", "path": "/sdcard/0/xxx.bmp" }
```

设备随后以 HTTP **PUT** 方式将 `path` 指向的文件流式上传到 `url`（`IOT::upload_file`）。

**环境数据（设备 → 服务器）**

```json
{ "timestamp": "20260101-120000", "temp": 25.3, "hum": 60.1 }
```

### 9.3 时间同步

- 启动时通过 `esp_netif_sntp` 向 `pool.ntp.org` 同步（最多重试 15 次）。
- 亦支持从服务器下发时间：MQTT 收到 `.../time` 主题后 `settimeofday()`。
- 时区固定为 `CST-8`（东八区）。
- 若时间未同步（时间戳不以 `2` 开头），上报时 `timestamp` 置空由服务器补全。

---

## 10. 用户界面

界面由 GUI Guider 生成（两个 screen），位于 `main/ui/`。

### screen_1（主监控界面）

| 控件 | 说明 |
|------|------|
| `screen_1_canvas` | 320×320 检测画面预览（叠加检测框） |
| `screen_1_Num` | 菌落计数 |
| `screen_1_Temperature` / `screen_1_Humidity` | 温湿度显示 |
| `screen_1_CO2` | CO₂ 显示（预留） |
| `screen_1_Time_until_next` | 距下次采集倒计时（预留） |
| `screen_1_led` | 污染指示灯（红=异常，绿=正常） |
| `screen_1_Button_screen1to2` | 跳转到设置界面 |

### screen_2（设置界面）

| 控件 | 说明 |
|------|------|
| `screen_2_Count_interval` | **采集间隔**下拉：30s / 10min / 30min |
| `screen_2_Upload_interval` | **上报间隔**下拉：1s / 15s / 30s |
| `screen_2_Temperature_slider` | 目标温度滑动条（回调已注释，未接入） |
| `screen_2_Button_screen2to1` | 返回主界面 |

交互回调定义在 `main/ui/custom.cpp`：

- `dropdown_capture_event_cb`：更新采集间隔，必要时 `xTaskAbortDelay` 立刻唤醒采集任务。
- `dropdown_report_env_event_cb`：更新环境上报间隔。
- `slider_temp_event_cb`：温度设定（当前注释，需配合 `tcd/` 温控模块使用）。

**LVGL 线程安全**：所有 UI 操作必须包裹 `lv_lock()` / `lv_unlock()`（定义于 `frontend.hpp`）。

---

## 11. 构建与烧录

### 11.1 前置要求

- ESP-IDF **v6.0.1**（已激活，`IDF_PATH` 环境变量可用）
- 目标芯片 `esp32p4`
- esp-clang 工具链（随 IDF 提供）

### 11.2 常用命令

```bash
idf.py build          # 编译
idf.py flash          # 烧录固件 + 分区表 + 模型/图片分区
idf.py monitor        # 串口监视
idf.py menuconfig     # 图形化配置
```

### 11.3 配置项（menuconfig）

**`IOT Configuration`**

| 配置 | 说明 |
|------|------|
| `IOT_MQTT_SERVER` | MQTT 服务器地址 |
| `IOT_MQTT_USERNAME` | MQTT 用户名 |
| `IOT_MQTT_PASSWORD` | MQTT 密码 |

**`SD Card`**

| 配置 | 说明 |
|------|------|
| `SD_CARD_SPI_MOD` | SPI 模式（默认） |
| `SD_CARD_SDIO_MOD` | SDIO 模式（当前标记为无效） |

> 编译产物中的 `build/compile_commands.json` 供 clangd 使用，编辑器配置见 `.vscode/settings.json`。

---

## 12. 分区表

`partitions.csv`：

| 名称 | 类型 | 子类型 | 偏移 | 大小 |
|------|------|--------|------|------|
| nvs | data | nvs | 0x9000 | 24K |
| phy_init | data | phy | 0xf000 | 4K |
| factory | app | factory | 0x10000 | 8M |
| yolo26 | data | spiffs | 自动 | 2600K |
| image | data | spiffs | 自动 | 640K |

- `yolo26` 分区存放模型 `main/yolo26/yolo26n_640.espdl`。
- `image` 分区存放示例图 `main/yolo26/12995.jpg`。
- 二者由 `main/CMakeLists.txt` 中的 `esptool_py_flash_to_partition()` 在 `flash` 阶段烧录，运行时通过 `esp_partition_mmap()` / `dl::Model` 直接映射读取。

---

## 13. 开发注意事项

1. **LVGL 全局锁**：多任务环境务必使用 `lv_lock()` / `lv_unlock()` 包裹 UI 操作，否则会随机崩溃。
2. **显示方向与镜像**：`LCD::ST7796` 提供 `set_mirror()` / `set_swap()`；修改面板镜像后需同步触摸坐标映射（当前 `main.cpp` 使用 `panel.set_swap(true)`）。
3. **SPIRAM 使用**：大缓冲（如 1080×1080 `img_t`、320×320 canvas）均从 SPIRAM 分配，需 16/128 字节对齐；涉及 CPU 与 DMA 交互的缓冲要调用 `esp_cache_msync()` 保持缓存一致。
4. **SHT3x 走 UART**：当前温湿度传感器使用 UART1 读取（输出格式 `R:049.3RH 026.8C`），并非 I2C；`sensors/sensor.h` 中的 SHT40 + SGP30 为另一套 I2C 驱动，当前未启用。
5. **`main/ui/` 自动生成**：除 `custom.cpp/.hpp` 外请勿手改，重新生成会覆盖。
6. **温控模块未接入**：`tcd/` 的 PID 加热控制已实现，但 `main.cpp` 中相关初始化与回调被注释，默认不运行。
7. **网络仅以太网**：Wi-Fi 相关 Kconfig 为历史遗留，请勿依赖。
8. **ESP32-P4 版本限制**：`sdkconfig.defaults` 指定 `CONFIG_ESP32P4_REV_MIN_100`，注意硬件版本匹配。
9. **事件循环唯一性**：以太网与 MQTT 均会创建默认事件循环，MQTT 初始化对已存在的循环做了容错处理。

---

## 14. 依赖组件

组件清单声明于 `main/idf_component.yml`，实际版本锁定于 `dependencies.lock`。共 19 个依赖组件（`managed_components/`）：

| 组件 | 用途 |
|------|------|
| `espressif/esp_video` | 视频/V4L2 框架（摄像头） |
| `espressif/esp_sccb_intf` | SCCB/I2C 摄像头控制接口 |
| `espressif/esp_cam_sensor` | 摄像头传感器驱动（含 OV5647） |
| `espressif/esp-dl` (3.3.6) | 深度学习推理框架 |
| `espressif/yolo26` | YOLO26 检测器实现 |
| `espressif/esp_ipa` | ISP 图像处理算法 |
| `espressif/esp_lcd_st7796` | ST7796 屏幕驱动 |
| `espressif/esp_lcd_touch_ft5x06` | FT5x06 系列触摸驱动（兼容 FT6336） |
| `espressif/esp_lcd_touch` | 触摸抽象层 |
| `espressif/ip101` | IP101 以太网 PHY 驱动 |
| `espressif/mqtt` | MQTT 客户端 |
| `espressif/cjson` | JSON 解析/序列化 |
| `espressif/esp_new_jpeg` | 硬件/软件 JPEG 编解码 |
| `espressif/esp_h264` | H.264 编解码（依赖项） |
| `espressif/dl_fft` | esp-dl 的 FFT 依赖 |
| `espressif/cmake_utilities` | 构建辅助脚本 |
| `espressif/usb` / `espressif/usb_host_uvc` | USB 主机/UVC（依赖项） |
| `lvgl/lvgl` (8.4.0) | 图形库 |
