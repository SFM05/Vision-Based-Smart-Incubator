#pragma once

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_panel_io.h"

#include "display_config.hpp"

class LvglPort
{
public:
    static esp_err_t init(
        esp_lcd_panel_handle_t panel,
        esp_lcd_panel_io_handle_t io,
        esp_lcd_touch_handle_t touch,
        uint16_t width,
        uint16_t height
    );
    static void tick();
    static void refresh();

    /**
     * @brief 同步镜像配置到触摸坐标映射
     *
     * 当调用 panel.set_mirror() 修改屏幕镜像后，必须调用此函数，
     * 否则触摸坐标会与显示内容错位。
     *
     * @param cfg 镜像配置（需与 ST7796::set_mirror 传入的值一致）
     *
     * @code
     *   panel.set_mirror({.mv=true, .mx=true, .my=false});
     *   LvglPort::set_mirror(panel.mirror());
     * @endcode
     */
    static void set_mirror(const MirrorConfig& cfg);
    /**
     * @brief 获取当前触摸使用的镜像配置
     *
     * @return const MirrorConfig& 镜像配置
     */
    static const MirrorConfig& mirror() { return s_mirror; }

private:
    static MirrorConfig s_mirror;
};
