#include <include/rtc64.h>
#include <include/config.h>
#include <stdint.h>

void draw_pixel(int x, int y, uint32_t color);
void draw_char(char c, int x, int y, uint32_t fg, uint32_t bg);

void rtc64_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            draw_pixel(x + j, y + i, color);
        }
    }
}

void rtc64_draw_text(const char* s, int x, int y, uint32_t color) {
    for (int i = 0; s[i] != '\0'; i++) {
        /* draw_char is character-based (x/8), we need pixel-based for WM */
        /* Temporary hack: scaling factors must match console.c */
        extern void draw_char_pixel(char c, int px, int py, uint32_t fg, uint32_t bg);
        draw_char_pixel(s[i], x + (i * 16), y, color, 0x000000);
    }
}

void rtc64_draw_gui_primitives(rtc64_window_t* win) {
    /* Title Bar */
    uint32_t bar_color = win->focused ? 0x0000AA : 0x555555;
    rtc64_draw_rect(win->x, win->y, win->w, 24, bar_color);

    /* Frame */
    rtc64_draw_rect(win->x, win->y + 24, 4, win->h - 24, 0xAAAAAA);
    rtc64_draw_rect(win->x + win->w - 4, win->y + 24, 4, win->h - 24, 0xAAAAAA);
    rtc64_draw_rect(win->x, win->y + win->h - 4, win->w, 4, 0xAAAAAA);

    /* Client Area Background */
    rtc64_draw_rect(win->x + 4, win->y + 24, win->w - 8, win->h - 28, 0xFFFFFF);
}
