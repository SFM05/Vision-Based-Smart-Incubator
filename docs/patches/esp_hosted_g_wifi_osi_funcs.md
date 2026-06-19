# ESP-Hosted `g_wifi_osi_funcs` 符号修复

## 问题

开机日志出现：
```
I (1326) net80211: OS adapter function version error! Version 8 is expected, but it is 0
```

## 根因

`g_wifi_osi_funcs` 被两个组件分别定义：

| 组件 | 文件 | 行 | 定义 | 段 |
|---|---|---|---|---|
| `esp_wifi` | `esp32p4/esp_adapter.c` | 699 | `wifi_osi_funcs_t g_wifi_osi_funcs = {._version = 8, ...}` | `.data` (D) |
| `espressif__esp_hosted` | `host/port/esp/freertos/src/port_esp_hosted_host_os.c` | 52 | `wifi_osi_funcs_t g_wifi_osi_funcs;` (暂定定义) | `.bss` (B) |

`port_esp_hosted_host_os.c:52` 的变量声明缺少 `extern`，导致编译器将其视为暂定定义（tentative definition），分配在 BSS 段（全零初始化）。链接后 BSS 版本覆盖了 `esp_adapter.c` 中正确的 DATA 版本。

WiFi 初始化时，`WIFI_INIT_CONFIG_DEFAULT()` 设置 `.osi_funcs = &g_wifi_osi_funcs`，该结构体的 `_version = 0`。配置序列化后发送给从机（ESP32-C6），从机 net80211 层检查版本时发现期望值 8 但收到 0，报出版本错误。

## 修复

在 `port_esp_hosted_host_os.c:51-52` 的变量声明前加 `extern`，使其变为纯声明而非定义：

```c
extern const wpa_crypto_funcs_t g_wifi_default_wpa_crypto_funcs;
extern wifi_osi_funcs_t g_wifi_osi_funcs;
```

修改后链接器不再使用此 BSS 定义，转而使用 `esp32p4/esp_adapter.c` 中的 `.data` 定义（`_version = 8`）。

## 受影响的组件

- **组件**: `espressif/esp_hosted`
- **版本**: 2.12.7
- **仓库**: https://github.com/espressif/esp-hosted-mcu
- **Commit**: `8610f4f5e45b3269dad150358695de8eddaa1c96`

**警告**: `idf.py update-dependencies` 会覆盖此修改，需重新应用。managed 组件在 `.gitignore` 中，不会被 git 跟踪，重装后请手动恢复。

## 验证

1. 编译后检查 `.obj` 中该符号应为 `U`（未定义引用）而非 `B`（BSS 定义）：
   ```bash
   riscv32-esp-elf-nm build/esp-idf/espressif__esp_hosted/CMakeFiles/__idf_espressif__esp_hosted.dir/ \
     host/port/esp/freertos/src/port_esp_hosted_host_os.c.obj | grep g_wifi_osi_funcs
   ```
2. 最终 ELF 中该符号应为 `D`（已初始化数据段）：
   ```bash
   riscv32-esp-elf-nm build/Vision-Based-Smart-Incubator.elf | grep g_wifi_osi_funcs
   ```
3. 运行时 `cfg.osi_funcs->_version` 应输出 8 而非 0，且 `net80211: OS adapter function version error` 消失。
