#define RSL_BINARY_MODE
#include <include/rsl.h>
#include <include/rtc64.h>

/* RSL Binary Header */
__attribute__((section(".header")))
rsl_header_t rsl_header = {
    .magic = {'R', 'S', 'L', '1'},
    .entry_offset = sizeof(rsl_header_t),
    .stack_size = 65536,
    .flags = 1 /* GUI-aware */
};

/* Internal WM State */
rtc64_window_t windows[MAX_WINDOWS];
int window_count = 0;

/* External dependencies provided by kernel via syscalls or shared memory */
/* For this proof of concept, we assume the WM has direct access to drawing if in kernel mode,
   but since this is a .bin, it will use syscalls for everything or kernel will call its internal functions.
   Actually, a WM app needs to be 'special'. */

void rtc64_init(void) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        windows[i].id = -1;
        windows[i].visible = false;
    }
    window_count = 0;
}

/* GUI Rendering Primitives (Standalone version) */
/* These will use kernel syscalls for pixel drawing in a real RSL app */
static inline void draw_pixel_sys(int x, int y, uint32_t color) {
    __asm__ volatile ("mov $10, %%rax; mov %0, %%rdi; mov %1, %%rsi; mov %2, %%rdx; int $0x03" : : "r"((uint64_t)x), "r"((uint64_t)y), "r"((uint64_t)color) : "rax", "rdi", "rsi", "rdx");
}

static inline void draw_char_pixel_sys(char c, int px, int py, uint32_t fg, uint32_t bg) {
    __asm__ volatile ("mov $11, %%rax; mov %0, %%rdi; mov %1, %%rsi; mov %2, %%rdx; mov %3, %%rcx; mov %4, %%r8; int $0x03" : : "r"((uint64_t)c), "r"((uint64_t)px), "r"((uint64_t)py), "r"((uint64_t)fg), "r"((uint64_t)bg) : "rax", "rdi", "rsi", "rdx", "rcx", "r8");
}

void rtc64_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            draw_pixel_sys(x + j, y + i, color);
        }
    }
}

void rtc64_draw_text(const char* s, int x, int y, uint32_t color) {
    for (int i = 0; s[i] != '\0'; i++) {
        draw_char_pixel_sys(s[i], x + (i * 16), y, color, 0x000000);
    }
}

void rtc64_draw_gui_primitives(rtc64_window_t* win) {
    uint32_t bar_color = win->focused ? 0x0000AA : 0x555555;
    rtc64_draw_rect(win->x, win->y, win->w, 24, bar_color);
    rtc64_draw_text(win->title, win->x + 8, win->y + 4, 0xFFFFFF);
    rtc64_draw_rect(win->x, win->y + 24, 4, win->h - 24, 0xAAAAAA);
    rtc64_draw_rect(win->x + win->w - 4, win->y + 24, 4, win->h - 24, 0xAAAAAA);
    rtc64_draw_rect(win->x, win->y + win->h - 4, win->w, 4, 0xAAAAAA);
    rtc64_draw_rect(win->x + 4, win->y + 24, win->w - 8, 20, 0xCCCCCC);
    for (int i = 0; i < win->menu_count; i++) {
        rtc64_draw_text(win->menus[i].label, win->x + 12 + (i * 64), win->y + 26, 0x000000);
    }
    rtc64_draw_rect(win->x + 4, win->y + 44, win->w - 8, win->h - 48, 0xFFFFFF);
    for (int i = 0; i < win->button_count; i++) {
        rtc64_button_t* btn = &win->buttons[i];
        rtc64_draw_rect(win->x + btn->x, win->y + btn->y, btn->w, btn->h, 0x888888);
        rtc64_draw_text(btn->label, win->x + btn->x + 4, win->y + btn->y + 2, 0xFFFFFF);
    }
}

void rtc64_draw_all(void) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].visible) {
            rtc64_draw_gui_primitives(&windows[i]);
            if (windows[i].on_draw) {
                windows[i].on_draw(windows[i].x + 4, windows[i].y + 44, windows[i].w - 8, windows[i].h - 48);
            }
        }
    }
    /* Mouse drawing should be handled by kernel or via special syscall */
}

void wm_main(void) {
    rtc64_init();
    rsl_print("[WM] RTC64 Window Manager Standalone Started.\n");

    while(1) {
        /* In a real standalone app, we'd poll events via syscalls */
        rtc64_draw_all();
        __asm__ volatile ("mov $1, %%rax; int $0x03" ::: "rax"); /* Syscall Yield */
    }
}

void _start(void) {
    wm_main();
}
