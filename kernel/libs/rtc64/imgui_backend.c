#include <include/rtc64.h>
#include <include/config.h>
#include <stdint.h>
#include <stddef.h>

void draw_pixel(int x, int y, uint32_t color);

/* Sovereign ImGui Backend: Forensic Pixel Renderer */

void rtc64_draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = (x2 - x1 >= 0) ? (x2 - x1) : (x1 - x2);
    int dy = (y2 - y1 >= 0) ? (y2 - y1) : (y1 - y2);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        draw_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void rtc64_draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color) {
    rtc64_draw_line(x1, y1, x2, y2, color);
    rtc64_draw_line(x2, y2, x3, y3, color);
    rtc64_draw_line(x3, y3, x1, y1, color);
}

void rtc64_blit(int x, int y, int w, int h, uint32_t* data) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            draw_pixel(x + j, y + i, data[i * w + j]);
        }
    }
}
