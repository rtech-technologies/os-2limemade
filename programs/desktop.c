#include <stdint.h>
#include <stdbool.h>
#include <include/rsl.h>

#define COLOR_BG      0x1A1A1A
#define COLOR_TASKBAR 0x333333
#define COLOR_TEXT    0xFFFFFF
#define COLOR_ICON    0x00FF88

void _start(void) {
    rsl_fb_t fb;
    if (rsl_get_fb(&fb) != 0) return;

    /* Desktop Background */
    gui_draw_rect(&fb, 0, 0, fb.width, fb.height, COLOR_BG);

    /* Taskbar */
    gui_draw_rect(&fb, 0, fb.height - 40, fb.width, 40, COLOR_TASKBAR);
    gui_draw_text(&fb, 10, fb.height - 25, "[ SOVEREIGN OS ]", COLOR_ICON);

    /* Icons */
    gui_draw_rect(&fb, 50, 50, 64, 64, COLOR_ICON);
    gui_draw_text(&fb, 50, 120, "Terminal", COLOR_TEXT);

    gui_draw_rect(&fb, 150, 50, 64, 64, COLOR_ICON);
    gui_draw_text(&fb, 150, 120, "Files", COLOR_TEXT);

    /* Tutorial Overlay */
    int tw = 400, th = 200;
    int tx = (fb.width - tw) / 2;
    int ty = (fb.height - th) / 2;
    gui_draw_rect(&fb, tx, ty, tw, th, 0x444444);
    gui_draw_text(&fb, tx + 20, ty + 20, "Welcome to Sovereign OS", COLOR_ICON);
    gui_draw_text(&fb, tx + 20, ty + 50, "1. Estates: Kernel, Trust, Script", COLOR_TEXT);
    gui_draw_text(&fb, tx + 20, ty + 80, "2. RSL: Sovereign Scripting", COLOR_TEXT);
    gui_draw_text(&fb, tx + 20, ty + 110, "3. Cargo: Installation Logistics", COLOR_TEXT);

    gui_draw_text(&fb, tx + 100, ty + 160, "[ PRESS 'S' TO START SHELL ]", 0x00AAFF);

    for (;;) {
        char c = rsl_get_char_nonblock();
        if (c == 's' || c == 'S') {
            void rsl_spawn(int module_idx, uint32_t slab_id, uint32_t uaid);
            rsl_spawn(2, 1, 100); /* Spawn Shell */
            break;
        }
        sys_yield();
    }

    for (;;) {
        sys_yield();
    }
}
