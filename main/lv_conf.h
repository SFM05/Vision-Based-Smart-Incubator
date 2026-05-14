#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        1
#define LV_COLOR_SCREEN_TRANSP  0

#define LV_HOR_RES_MAX          480
#define LV_VER_RES_MAX          320

#define LV_DPI_DEF              100

#define LV_USE_PERF_MONITOR     0
#define LV_USE_MEM_MONITOR      0

#define LV_ARCH_64              0

#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1

#define LV_USE_LOG              0

#define LV_USE_ANIMATION        1

#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_20   1
#define LV_FONT_DEFAULT         12

#define LV_USE_IMG              1
#define LV_USE_CANVAS           1
#define LV_USE_LABEL            1
#define LV_USE_SLIDER           1
#define LV_USE_BTN              1

#define LV_USE_THEME_DEFAULT    1
#define LV_THEME_DEFAULT_DARK   1
#define LV_THEME_DEFAULT_GROW   0

#define LV_USE_CUSTOM_RES       0

#define LV_MEM_CUSTOM           1
#if LV_MEM_CUSTOM
#  define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#  define LV_MEM_CUSTOM_ALLOC   malloc
#  define LV_MEM_CUSTOM_FREE    free
#  define LV_MEM_CUSTOM_REALLOC realloc
#endif

#define LV_DISP_DEF_REFR_PERIOD 30
#define LV_INDEV_DEF_READ_PERIOD 30

#define LV_USE_GRID             1
#define LV_USE_FLEX             1

#define LV_USE_DRAW_SW          1
#define LV_DRAW_SW_DMA_NUM      1

#endif
