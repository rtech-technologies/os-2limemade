#include <stdint.h>
#include <stdbool.h>
#include <include/rsl.h>
#include <include/stdlib.h>

#define list_disks rsl_list_disks
#define partition_disk rsl_partition_disk
#define format_disk rsl_format_disk
#define mkdir(p) rsl_mkdir(str_create(p))
#define write_file(p, c) rsl_write(str_create(p), str_create(c))
#define open_file(p, m) rsl_open(str_create(p), m)
#define read_file rsl_read
#define close_file rsl_close
#define exists(p) rsl_exists(str_create(p))

typedef vfs_handle_user_t vfs_handle_t;

#define COLOR_BG      0x222222
#define COLOR_PANEL   0x444444
#define COLOR_TEXT    0xFFFFFF
#define COLOR_PRIMARY 0x00FF88
#define COLOR_WARNING 0xFF5555

const char* art[] = {
    "  /\\_/\\  \n ( o.o ) \n  > ^ <  ",
    "  _____  \n |     | \n |_____| ",
    "  S O V  \n  E R N  \n  E I G  "
};
int current_art = 0;
bool debug_overlay = false;

void draw_cargo_icon(rsl_fb_t* fb, int x, int y) {
    gui_draw_rect(fb, x, y, 10, 10, COLOR_PRIMARY);
    gui_draw_rect(fb, x+2, y+2, 6, 6, COLOR_BG);
}

void _start(void) {
    rsl_fb_t fb;
    if (rsl_get_fb(&fb) != 0) {
        print("CRITICAL: GOP Unavailable.\n");
        while(1) __asm__ volatile ("pause");
    }

    int panel_w = 600, panel_h = 450;
    int px = (fb.width - panel_w) / 2, py = (fb.height - panel_h) / 2;

    while (1) {
        gui_draw_rect(&fb, 0, 0, fb.width, fb.height, COLOR_BG);
        gui_draw_rect(&fb, px, py, panel_w, panel_h, COLOR_PANEL);
        gui_draw_text(&fb, px + 100, py + 30, "[ SOVEREIGN CARGO: INSTALLER & MANAGER ]", COLOR_PRIMARY);
        gui_draw_text(&fb, px + 520, py + 10, "v2.0", 0xAAAAAA);

        if (current_art > 0)
            gui_draw_text(&fb, px + 450, py + 50, art[current_art-1], COLOR_TEXT);

        gui_draw_text(&fb, px + 80, py + 100, "1. FRESH INSTALL (WIPES DISK)", COLOR_TEXT);
        gui_draw_text(&fb, px + 80, py + 130, "2. UPDATE/UPGRADE (PRESERVES USERS)", COLOR_TEXT);
        gui_draw_text(&fb, px + 80, py + 160, "3. RESTORE (FROM /recovery/)", COLOR_TEXT);

        if (debug_overlay) {
            gui_draw_rect(&fb, 10, 10, 200, 100, 0x000000);
            gui_draw_text(&fb, 20, 20, "DEBUG MODE ACTIVE", 0xFFFF00);
            gui_draw_text(&fb, 20, 40, "Handover: OK", 0x00FF00);
        }

        char cmd_buf[16];
        rsl_input("Selection: ", cmd_buf);
        char c = cmd_buf[0];

        if (c == 'a') { current_art = (current_art + 1) % 4; continue; }
        if (c == 'd') { debug_overlay = !debug_overlay; continue; }

        int mode = c - '0';
        if (mode < 1 || mode > 3) continue;

        int disks = list_disks();
        for (int i = 0; i < disks; i++) {
            char name[32]; uint64_t size;
            rsl_get_disk_info(i, name, &size);
            gui_draw_text(&fb, px + 100, py + 220 + (i * 20), name, COLOR_TEXT);
        }

        rsl_input("Disk Index: ", cmd_buf);
        int target_disk = cmd_buf[0] - '0';

        if (mode == 1) {
            gui_draw_text(&fb, px + 50, py + 400, "INSTALLING... ", COLOR_PRIMARY);
            for (int p = 0; p < 20; p++) {
                gui_draw_rect(&fb, px + 150 + (p * 15), py + 400, 10, 10, COLOR_PRIMARY);
                draw_cargo_icon(&fb, px + 150 + (p * 15), py + 415);
                for(int j=0; j<50; j++) sys_yield();
            }

            if (partition_disk(target_disk) == 0 && format_disk(target_disk) == 0) {
                int res;
                __asm__ volatile ("int $3" : : "a"((uint64_t)301), "b"((uint64_t)target_disk), "c"((uint64_t)"DATA"), "d"((uint64_t)&res) : "memory");
                mkdir("DATA:/sys");
                mkdir("DATA:/users");
                mkdir("DATA:/recovery");
                mkdir("DATA:/var");
                mkdir("DATA:/var/log");

                /* Create SYSTEM user: UUID 0, no user folder */
                write_file("DATA:/sys/users.jsonl", "{\"user\":\"system\",\"uid\":0,\"uuid\":\"0\"}\n");

                write_file("DATA:/sys/.installed", "{\"version\":\"2.0\",\"type\":\"fresh\",\"uuid\":\"0\"}");
                gui_draw_text(&fb, px + 50, py + 400, "SUCCESS. PRESS ENTER TO REBOOT.          ", COLOR_PRIMARY);
            }
        }

        rsl_input("", cmd_buf);
        for (;;) __asm__ volatile ("int $0x81");
    }
}
