#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

#define FT6336_MAX_TOUCH_POINTS  2

typedef struct {
    int16_t x;
    int16_t y;
    bool    active;
} touch_point_t;

void ft6336_init(void);
bool ft6336_available(void);
int  ft6336_read_points(touch_point_t points[FT6336_MAX_TOUCH_POINTS]);

#ifdef __cplusplus
}
#endif
