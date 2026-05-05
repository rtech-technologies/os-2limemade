#include <include/rsl.h>
#include <include/nuklear_rtc64.h>

void desktop_main(void) {
    nk_rect_t win = {50, 50, 400, 300};
    nk_color_t bg = {33, 33, 33, 255};
    nk_color_t fg = {0, 255, 136, 255};

    while (1) {
        nk_rtc64_draw_rect(win, bg);
        nk_rtc64_draw_text(60, 60, "OS*2 Limemade Desktop", 21, fg);
        nk_rtc64_draw_text(60, 90, "1. Architecture Overview", 24, fg);
        nk_rtc64_draw_text(60, 110, "2. RSL Syscall Flow", 19, fg);
        nk_rtc64_draw_text(60, 150, "Tutorial: OS*2 uses Active-Relay multitasking.", 44, fg);
        nk_rtc64_draw_text(60, 180, "License: RTECH Sovereign (c) 2024", 33, fg);

        void delta_move_flush(void);
        delta_move_flush();

        /* Voluntary yield to scheduler */
        void sys_yield(void);
        sys_yield();
    }
}
