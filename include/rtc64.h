#ifndef RTC64_H
#define RTC64_H

#include <stdint.h>

typedef struct {
    int x, y, w, h;
    uint32_t color;
} rtc64_rect_t;

void rtc64_draw_pixel(int x, int y, uint32_t color);
void rtc64_draw_rect(int x, int y, int w, int h, uint32_t color);
void rtc64_draw_line(int x1, int y1, int x2, int y2, uint32_t color);
void rtc64_draw_text(const char* s, int x, int y, uint32_t color);

#endif
