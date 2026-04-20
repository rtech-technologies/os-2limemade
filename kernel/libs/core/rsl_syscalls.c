#include <include/rsl.h>
#include <stdint.h>

void draw_char_at(char c, int x, int y, uint8_t attr);
void draw_pixel(int x, int y, uint32_t color);

void rsl_syscall_handler(uint64_t id, uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e) {
    (void)e;
    switch (id) {
        case 0: print((const char*)a); break;
        case 1: set_color((color_t)a, (color_t)b); break;
        case 10:
            /* Bounds check for security */
            if (a < 1024 && b < 768) draw_pixel((int)a, (int)b, (uint32_t)c);
            break;
        case 11:
            /* Bounds check for security */
            if (b < 80 && c < 24) draw_char_at((char)a, (int)b, (int)c, (uint8_t)d);
            break;
    }
}
