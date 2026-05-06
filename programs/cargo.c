#include <stdint.h>
#include <stdbool.h>
#include <include/rsl.h>


int list_disks(void) {
    volatile int count = 0;
    __asm__ volatile ("int $3" : : "a"((uint64_t)101), "b"((uint64_t)&count) : "memory");
    return count;
}

void get_disk_info(int id, char* name, uint64_t* size) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)103), "b"((uint64_t)id), "c"((uint64_t)name), "d"((uint64_t)size) : "memory");
}

int partition_disk(int id) {
    volatile int res = -1;
    __asm__ volatile ("int $3" : : "a"((uint64_t)105), "b"((uint64_t)id), "c"((uint64_t)&res) : "memory");
    return res;
}

int format_disk(int id) {
    volatile int res = -1;
    __asm__ volatile ("int $3" : : "a"((uint64_t)104), "b"((uint64_t)id), "c"((uint64_t)&res) : "memory");
    return res;
}

void mkdir(const char* path) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)121), "b"((uint64_t)path) : "memory");
}

void write_file(const char* path, const char* content) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)120), "b"((uint64_t)path), "c"((uint64_t)content) : "memory");
}

uint64_t hash_password(const char* pass) {
    volatile uint64_t h = 0;
    __asm__ volatile ("int $3" : : "a"((uint64_t)130), "b"((uint64_t)pass), "c"((uint64_t)&h) : "memory");
    return h;
}

#define COLOR_BG      0x222222
#define COLOR_PANEL   0x444444
#define COLOR_TEXT    0xFFFFFF
#define COLOR_PRIMARY 0x00FF88

void _start(void) {
    rsl_fb_t fb;
    if (rsl_get_fb(&fb) != 0) {
        print("CRITICAL: GOP Unavailable.\n");
        while(1) __asm__ volatile ("pause");
    }

    /* Background */
    gui_draw_rect(&fb, 0, 0, fb.width, fb.height, COLOR_BG);

    /* Main Panel (Rounded-ish via offset) */
    int panel_w = 600;
    int panel_h = 400;
    int px = (fb.width - panel_w) / 2;
    int py = (fb.height - panel_h) / 2;
    gui_draw_rect(&fb, px, py, panel_w, panel_h, COLOR_PANEL);

    /* Text from installer.png */
    gui_draw_text(&fb, px + 100, py + 50, "Have you used os*2? before?", COLOR_TEXT);

    const char* options[] = {
        "yes but I don't want a tutorial",
        "yes but I'd like a tutorial",
        "No  but I don't want a tutorial",
        "no  but I'd like a tutorial"
    };

    for (int i = 0; i < 4; i++) {
        /* Radio Button */
        gui_draw_rect(&fb, px + 80, py + 100 + (i * 30), 16, 16, COLOR_TEXT);
        if (i == 0) gui_draw_rect(&fb, px + 82, py + 102 + (i * 30), 12, 12, COLOR_BG);

        gui_draw_text(&fb, px + 110, py + 105 + (i * 30), options[i], COLOR_TEXT);
    }

    /* Note box */
    gui_draw_rect(&fb, px + 60, py + 300, 480, 80, 0x333333);
    gui_draw_text(&fb, px + 70, py + 310, "note:", COLOR_TEXT);
    gui_draw_text(&fb, px + 70, py + 330, "this tutorial will teach you how to use", COLOR_TEXT);
    gui_draw_text(&fb, px + 70, py + 350, "this os to its fullest", COLOR_TEXT);

    /* Proceed with actual installation logic in background/fallback */
    print("[CARGO] UI Loaded. Waiting for hardware handshake...\n");

    /* License Enforcement */
    gui_draw_rect(&fb, px + 50, py + 380, 500, 15, COLOR_PANEL);
    gui_draw_text(&fb, px + 60, py + 382, "Licensed under Sovereign Estates Protocol - Build 2024-05-23", 0xAAAAAA);

    int disks = list_disks();
    if (disks <= 0) {
        gui_draw_text(&fb, px + 70, py + 370, "ERROR: NO DISKS FOUND", 0xFF5555);
        while(1) __asm__ volatile ("pause");
    }

    /* Auto-installer logic (Simplified for demo) */
    int target_disk = 0;
    int res = -1;
    if (partition_disk(target_disk) == 0 && format_disk(target_disk) == 0) {
        /* Force mount the newly formatted SATA disk as DATA for OOBE */
        __asm__ volatile ("int $3" : : "a"((uint64_t)301), "b"((uint64_t)target_disk), "c"((uint64_t)"DATA"), "d"((uint64_t)&res) : "memory");

        mkdir("DATA:/users");
        mkdir("DATA:/bin");
        mkdir("DATA:/sys");
        write_file("DATA:/CHANGELOG.txt", "SYSTEM INSTALLED VIA GUI\n");

        /* Create default user 'sovereign' (Syscall 112) */
        __asm__ volatile ("int $3" : : "a"((uint64_t)112), "b"((uint64_t)"sovereign"), "c"((uint64_t)"password"), "d"((uint64_t)&res) : "memory");

        print("[CARGO] Installation payload delivered to SATA HDD.\n");
    }

    gui_draw_text(&fb, px + 350, py + 370, "[ PRESS ENTER TO REBOOT ]", COLOR_PRIMARY);

    char dummy[16];
    rsl_input("", dummy);

    for (;;) {
        __asm__ volatile ("int $0x81");
    }
}
