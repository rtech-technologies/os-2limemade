#include <include/rtc64.h>
#include <stdint.h>

extern void draw_pixel(int x, int y, uint32_t color);

void rtc64_draw_pixel(int x, int y, uint32_t color) {
    draw_pixel(x, y, color);
}

void rtc64_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            draw_pixel(x + j, y + i, color);
        }
    }
}

void rtc64_draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = (x2 - x1 < 0) ? x1 - x2 : x2 - x1;
    int dy = (y2 - y1 < 0) ? y1 - y2 : y2 - y1;
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        draw_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

extern void draw_char(char c, int x, int y, uint32_t fg, uint32_t bg);

void rtc64_draw_text(const char* s, int x, int y, uint32_t color) {
    for (int i = 0; s[i] != '\0'; i++) {
        /* Temporary: Use character-cell draw_char but with pixel-to-cell scaling */
        draw_char(s[i], (x + i * 8) / 16, y / 16, color, 0x000000);
    }
}
