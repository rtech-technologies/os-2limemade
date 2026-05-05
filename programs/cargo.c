#include <stdint.h>
#include <stdbool.h>
#include <include/rsl.h>

void rsl_input(const char* prompt, char* buffer) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)1), "b"((uint64_t)prompt), "c"((uint64_t)buffer) : "memory");
}

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

void run_program(const char* path) {
     void* curdir = str_create("/");
     bool safe = false;
     char cmd[128] = "run ";
     int i = 4, j = 0;
     while(path[j]) cmd[i++] = path[j++];
     cmd[i] = '\0';
     rsl_dispatch_command(cmd, &curdir, &safe);
}

#define COLOR_BG      0x222222
#define COLOR_PANEL   0x444444
#define COLOR_TEXT    0xFFFFFF
#define COLOR_PRIMARY 0x00FF88
#define COLOR_SUCCESS 0x00FF00

void _start(void) {
    rsl_fb_t fb;
    if (rsl_get_fb(&fb) != 0) {
        print("CRITICAL: GOP Unavailable.\n");
        while(1) __asm__ volatile ("pause");
    }

    /* Background */
    gui_draw_rect(&fb, 0, 0, fb.width, fb.height, COLOR_BG);

    /* Main Panel */
    int panel_w = 600;
    int panel_h = 400;
    int px = (fb.width - panel_w) / 2;
    int py = (fb.height - panel_h) / 2;
    gui_draw_rect(&fb, px, py, panel_w, panel_h, COLOR_PANEL);

    gui_draw_text(&fb, px + 50, py + 30, "OSx2 SOVEREIGN INSTALLER", COLOR_PRIMARY);
    gui_draw_text(&fb, px + 50, py + 70, "1. Disk Partitioning & GPT Ritual", COLOR_TEXT);
    gui_draw_text(&fb, px + 50, py + 90, "2. File System Handshake (FAT32)", COLOR_TEXT);
    gui_draw_text(&fb, px + 50, py + 110, "3. Initial System Payload Delivery", COLOR_TEXT);
    gui_draw_text(&fb, px + 50, py + 130, "4. Local Administrator Creation", COLOR_TEXT);

    print("[CARGO] Installation Sequence Initialized.\n");

    int disks = list_disks();
    if (disks <= 0) {
        gui_draw_text(&fb, px + 50, py + 160, "ERROR: NO MECHANICAL DRIVES FOUND", 0xFF5555);
        while(1) __asm__ volatile ("pause");
    }

    /* Target disk 0 for Sovereign Home */
    int target = 0;
    print("[CARGO] Partitioning Disk 0...\n");
    if (partition_disk(target) != 0) {
        gui_draw_text(&fb, px + 50, py + 160, "FAILED: GPT PARTITION RITUAL", 0xFF5555);
        while(1) __asm__ volatile ("pause");
    }

    print("[CARGO] Formatting Partition...\n");
    if (format_disk(target) != 0) {
        gui_draw_text(&fb, px + 50, py + 160, "FAILED: FS HANDSHAKE", 0xFF5555);
        while(1) __asm__ volatile ("pause");
    }

    /* Deliver Payload */
    mkdir("BOOT:/users");
    mkdir("BOOT:/bin");
    write_file("BOOT:/CHANGELOG.txt", "SYSTEM INSTALLED VIA CARGO\n");

    /* Administrator Creation Step */
    gui_draw_text(&fb, px + 50, py + 200, "Proceeding to User Creation...", COLOR_PRIMARY);
    print("[CARGO] Handing over to User Creator...\n");

    run_program("INITRD:/bin/user_creator.bin");

    gui_draw_text(&fb, px + 50, py + 300, "INSTALLATION COMPLETE.", COLOR_SUCCESS);
    gui_draw_text(&fb, px + 50, py + 330, "PRESS ENTER TO REBOOT INTO SOVEREIGN OS", COLOR_PRIMARY);

    char dummy[16];
    rsl_input("", dummy);

    for (;;) {
        __asm__ volatile ("int $0x81");
    }
}
