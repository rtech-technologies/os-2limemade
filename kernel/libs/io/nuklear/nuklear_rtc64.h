/*
 * Nuklear RTC64 Implementation
 * Commit: 83f7e1b (Authoritative)
 * URL: https://github.com/vurtun/nuklear (RTC64 fork)
 */

#ifndef NUKLEAR_RTC64_H
#define NUKLEAR_RTC64_H

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION

/* RTC64 Internal Drawer mapping */
void nk_rtc64_draw_line(int x0, int y0, int x1, int y1, uint32_t color);
void nk_rtc64_draw_rect(int x, int y, int w, int h, uint32_t color);
void nk_rtc64_draw_text(int x, int y, const char* text, int len, uint32_t color);

#endif
