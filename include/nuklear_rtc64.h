#ifndef NUKLEAR_RTC64_H
#define NUKLEAR_RTC64_H

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT

/* RTC64 Modified Nuklear Header (Stub) */
/* Commit: 83f7e1b (RTC64 Drawer Integration) */

typedef struct { int x, y, w, h; } nk_rect_t;
typedef struct { uint8_t r, g, b, a; } nk_color_t;

void nk_rtc64_draw_line(int x0, int y0, int x1, int y1, nk_color_t col);
void nk_rtc64_draw_rect(nk_rect_t rect, nk_color_t col);
void nk_rtc64_draw_text(int x, int y, const char* text, int len, nk_color_t col);

#endif
