#include <include/rtc64.h>
#include <include/mouse.h>
#include <include/rsl.h>
#include <kernel/unice64/task.h>
#include <stdint.h>
#include <stddef.h>

rtc64_window_t windows[MAX_WINDOWS];
int window_count = 0;

void rtc64_init(void) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        windows[i].id = -1;
        windows[i].visible = false;
    }
    window_count = 0;
}

int rtc64_create_window(int x, int y, int w, int h, const char* title) {
    if (window_count >= MAX_WINDOWS) return -1;

    int id = window_count++;
    windows[id].id = id;
    windows[id].x = x;
    windows[id].y = y;
    windows[id].w = w;
    windows[id].h = h;
    windows[id].title = title;
    windows[id].visible = true;
    windows[id].focused = true;
    windows[id].dragging = false;
    windows[id].menu_count = 0;
    windows[id].button_count = 0;
    windows[id].on_draw = NULL;
    windows[id].on_event = NULL;

    return id;
}

void rtc64_draw_gui_primitives(rtc64_window_t* win);

void vga_draw_mouse(int x, int y);

void rtc64_draw_all(void) {
    /* Draw windows from back to front */
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].visible) {
            rtc64_draw_gui_primitives(&windows[i]);
            if (windows[i].on_draw) {
                windows[i].on_draw(windows[i].x + 4, windows[i].y + 44, windows[i].w - 8, windows[i].h - 48);
            }
        }
    }

    /* Topmost Layer: Mouse Cursor */
    mouse_state_t* ms = get_mouse_state();
    if (ms && ms->active) {
        vga_draw_mouse(ms->x, ms->y);
    }
}

void rtc64_update(void) {
    mouse_state_t* ms = get_mouse_state();
    if (!ms || !ms->active) return;

    static bool last_left = false;
    static int lx = -1, ly = -1;

    rtc64_event_t ev = { .type = RTC64_EVENT_NONE, .x = ms->x, .y = ms->y };

    if (ms->left_button && !last_left) ev.type = RTC64_EVENT_MOUSE_DOWN;
    else if (!ms->left_button && last_left) ev.type = RTC64_EVENT_MOUSE_UP;
    else ev.type = RTC64_EVENT_MOUSE_MOVE;

    int dx = (lx == -1) ? 0 : (ms->x - lx);
    int dy = (ly == -1) ? 0 : (ms->y - ly);

    last_left = ms->left_button;
    lx = ms->x; ly = ms->y;

    /* Process windows from front to back (hit testing) */
    for (int i = MAX_WINDOWS - 1; i >= 0; i--) {
        if (!windows[i].visible) continue;

        rtc64_window_t* win = &windows[i];
        bool in_win = (ms->x >= win->x && ms->x < win->x + win->w &&
                       ms->y >= win->y && ms->y < win->y + win->h);

        if (ev.type == RTC64_EVENT_MOUSE_DOWN && in_win) {
            /* Focus window */
            for(int j=0; j<MAX_WINDOWS; j++) windows[j].focused = false;
            win->focused = true;

            /* Move window to front */
            if (i < window_count - 1) {
                rtc64_window_t tmp = windows[i];
                for (int k = i; k < window_count - 1; k++) windows[k] = windows[k+1];
                windows[window_count - 1] = tmp;
                win = &windows[window_count - 1];
            }

            /* Drag check (title bar) */
            if (ms->y < win->y + 24) win->dragging = true;

            /* Button Clicks */
            for (int b = 0; b < win->button_count; b++) {
                rtc64_button_t* btn = &win->buttons[b];
                int bx = win->x + btn->x;
                int by = win->y + btn->y;
                if (ms->x >= bx && ms->x < bx + btn->w && ms->y >= by && ms->y < by + btn->h) {
                    if (btn->handler) btn->handler();
                }
            }

            /* Menu Clicks */
            if (ms->y >= win->y + 24 && ms->y < win->y + 44) {
                int menu_idx = (ms->x - (win->x + 12)) / 64;
                if (menu_idx >= 0 && menu_idx < win->menu_count) {
                    /* For now, just trigger first item handler as 'simple' menu */
                    if (win->menus[menu_idx].item_count > 0 && win->menus[menu_idx].items[0].handler) {
                        win->menus[menu_idx].items[0].handler();
                    }
                }
            }
        }

        if (ev.type == RTC64_EVENT_MOUSE_UP) win->dragging = false;

        if (win->dragging) {
            win->x += dx;
            win->y += dy;
        }

        if (in_win && win->on_event) win->on_event(ev);
    }
}

void rtc64_wm_task(void) {
    rtc64_init();
    void vga_print(const char* fmt, ...);
    vga_print("[RTC64] Windowing Engine Online.\n");

    while(1) {
        rtc64_update();
        rtc64_draw_all();
        sys_yield();
    }
}
