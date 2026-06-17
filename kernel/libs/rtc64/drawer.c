#include <stdint.h>
#include <stddef.h>
#include <limine.h>

extern uint32_t* get_virtual_buffer(void);
extern struct limine_framebuffer_response* get_framebuffer(void);

void rtc64_draw_pixel(int x, int y, uint32_t color) {
    struct limine_framebuffer_response* resp = get_framebuffer();
    if (!resp || resp->framebuffer_count == 0) return;
    struct limine_framebuffer* fb = resp->framebuffers[0];
    uint32_t* buf = get_virtual_buffer();
    if (!buf) return;

    if (x >= 0 && (uint64_t)x < fb->width && y >= 0 && (uint64_t)y < fb->height) {
        buf[y * (fb->pitch / 4) + x] = color;
    }
}

void rtc64_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            rtc64_draw_pixel(x + j, y + i, color);
        }
    }
}
