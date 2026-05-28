#pragma once

#include <cstdint>

/**
 * @brief 屏幕镜像配置，对应 MADCTL 寄存器的 MV/MX/MY 位
 *
 * ST7796 的 MADCTL 位分配（与 esp_lcd_st7796 驱动一致）:
 *   mv (bit 5) — Row/Column Exchange，0=正常 1=交换（横屏）
 *   mx (bit 6) — Column Address Order，0=左→右 1=右→左（X 轴镜像）
 *   my (bit 7) — Row Address Order，0=上→下 1=下→上（Y 轴镜像）
 *
 * 修改镜像后必须同步调用 LvglPort::set_mirror() 更新触摸映射，否则触摸会错位。
 * 示例:
 * @code
 *   panel.set_mirror({.mv=true, .mx=true, .my=false});   // 仅 X 镜像
 *   LvglPort::set_mirror(panel.mirror());                 // 必须同步
 * @endcode
 */
struct MirrorConfig {
    bool mv = true;
    bool mx = true;
    bool my = true;

    /**
     * @brief 将配置转换为 MADCTL 寄存器值（命令 0x36）
     *
     * @return uint8_t MADCTL 值，含 BGR=1 位，默认 0xE8
     */
    uint8_t to_madctl() const {
        return 0x08
             | (mv ? 0x20 : 0)   // ST7796: MV = bit 5
             | (mx ? 0x40 : 0)   // ST7796: MX = bit 6
             | (my ? 0x80 : 0);  // ST7796: MY = bit 7
    }

    /**
     * @brief 从 MADCTL 寄存器值解析配置
     *
     * @param madctl MADCTL 寄存器值
     * @return MirrorConfig 解析结果
     */
    static MirrorConfig from_madctl(uint8_t madctl) {
        return {
            .mv = (madctl & 0x20) != 0,  // ST7796: MV = bit 5
            .mx = (madctl & 0x40) != 0,  // ST7796: MX = bit 6
            .my = (madctl & 0x80) != 0   // ST7796: MY = bit 7
        };
    }
};
