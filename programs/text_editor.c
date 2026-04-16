#include <include/rtc64.h>
#include <include/rsl.h>
#include <include/vfs.h>
#include <include/config.h>
#include <kernel/unice64/task.h>
#include <stdint.h>
#include <stddef.h>

void vga_print(const char* fmt, ...);

#define EDITOR_BUF_SIZE 2048
static char editor_buffer[EDITOR_BUF_SIZE];
static int editor_cursor = 0;

void editor_on_draw(int x, int y, int w, int h) {
    (void)w; (void)h;
    rtc64_draw_text(editor_buffer, x + 8, y + 8, 0x000000);
}

void editor_save(void) {
    void* path = str_create("DISK0:/editor.txt");
    vfs_handle_t* h = vfs_open(path, "w");
    if (h) {
        vfs_write(h, editor_buffer, editor_cursor);
        vfs_close(h);
        vga_print("[EDITOR] Saved to DISK0:/editor.txt\n");
    }
    release(path);
}

void editor_on_event(rtc64_event_t ev) {
    if (ev.type == RTC64_EVENT_KEY_DOWN) {
        if (ev.key == '\b') {
            if (editor_cursor > 0) editor_buffer[--editor_cursor] = '\0';
        } else if (ev.key == '\n' || ev.key == '\r') {
             if (editor_cursor < EDITOR_BUF_SIZE - 1) {
                editor_buffer[editor_cursor++] = '\n';
                editor_buffer[editor_cursor] = '\0';
            }
        } else if (ev.key >= 32 && ev.key < 127) {
            if (editor_cursor < EDITOR_BUF_SIZE - 1) {
                editor_buffer[editor_cursor++] = ev.key;
                editor_buffer[editor_cursor] = '\0';
            }
        }
    }
}

void text_editor_task(void) {
    for(int i=0; i<EDITOR_BUF_SIZE; i++) editor_buffer[i] = 0;

    char window_pop_char(void);

    int win_id = rtc64_create_window(50, 50, 400, 300, "Sovereign Text Editor");
    if (win_id != -1) {
        extern rtc64_window_t windows[];
        windows[win_id].on_draw = editor_on_draw;
        windows[win_id].on_event = editor_on_event;

        /* Add Save Button */
        windows[win_id].buttons[0].id = 1;
        windows[win_id].buttons[0].label = "SAVE";
        windows[win_id].buttons[0].x = 340;
        windows[win_id].buttons[0].y = 4;
        windows[win_id].buttons[0].w = 50;
        windows[win_id].buttons[0].h = 16;
        windows[win_id].buttons[0].handler = editor_save;
        windows[win_id].button_count = 1;

        /* Add File Menu */
        windows[win_id].menus[0].label = "FILE";
        windows[win_id].menus[0].items[0].label = "SAVE";
        windows[win_id].menus[0].items[0].handler = editor_save;
        windows[win_id].menus[0].item_count = 1;
        windows[win_id].menu_count = 1;
    }

    while(1) {
        /* Route window keyboard input to focused window */
        char c = window_pop_char();
        if (c != 0) {
            rtc64_event_t kev = { .type = RTC64_EVENT_KEY_DOWN, .key = c };
            extern rtc64_window_t windows[];
            for(int i=0; i<MAX_WINDOWS; i++) {
                if (windows[i].visible && windows[i].focused && windows[i].on_event) {
                    windows[i].on_event(kev);
                }
            }
        }
        sys_yield();
    }
}
