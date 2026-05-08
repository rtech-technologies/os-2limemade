#include <stdint.h>
#include <stdbool.h>
#include <include/rsl.h>

typedef void* vfs_handle_t;

vfs_handle_t open_file(const char* path, const char* mode) {
    vfs_handle_t h = NULL;
    __asm__ volatile ("int $3" : : "a"((uint64_t)122), "b"((uint64_t)path), "c"((uint64_t)mode), "d"((uint64_t)&h) : "memory");
    return h;
}

int read_file(vfs_handle_t h, void* buf, int len) {
    volatile int br = -1;
    __asm__ volatile ("int $3" : : "a"((uint64_t)123), "b"((uint64_t)h), "c"((uint64_t)buf), "d"((uint64_t)&br), "S"((uint64_t)len) : "memory");
    return br;
}

void close_file(vfs_handle_t h) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)124), "b"((uint64_t)h) : "memory");
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

    /* Selection Logic */
    int target_disk = -1;
    for (int i = 0; i < disks; i++) {
        char name[32];
        uint64_t size;
        get_disk_info(i, name, &size);

        /* Simple manual int-to-string for size */
        int size_mb = (int)(size / 1024 / 1024);

        gui_draw_text(&fb, px + 100, py + 250 + (i * 20), name, COLOR_TEXT);
        /* Target priority: NVMe > SATA > Other */
        if (target_disk == -1 && size_mb > 0) target_disk = i;
        if (name[0] == 'N' && name[1] == 'V' && size_mb > 0) target_disk = i;
    }

    if (target_disk == -1) target_disk = 0;

    /* Auto-installer logic (Simplified for demo) */
    int res = -1;
    print("[CARGO] Selected target disk index: ");
    /* Simple integer print would be nice here but keeping it minimal */

    gui_draw_text(&fb, px + 50, py + 370, "PREPARING DISK...", COLOR_PRIMARY);
    if (partition_disk(target_disk) == 0 && format_disk(target_disk) == 0) {
        /* Force mount the newly formatted disk as DATA for OOBE */
        __asm__ volatile ("int $3" : : "a"((uint64_t)301), "b"((uint64_t)target_disk), "c"((uint64_t)"DATA"), "d"((uint64_t)&res) : "memory");

        gui_draw_text(&fb, px + 50, py + 370, "DEPLOYING SOVEREIGN ROOT...          ", COLOR_PRIMARY);
        mkdir("DATA:/sys");
        mkdir("DATA:/var");
        mkdir("DATA:/var/log");
        mkdir("DATA:/var/crash");
        mkdir("DATA:/users");
        mkdir("DATA:/bin");

        write_file("DATA:/var/log/install.log", "[CARGO] Sovereign Installation Started\n");
        write_file("DATA:/CHANGELOG.txt", "SYSTEM INSTALLED VIA GUI - OSX2 UPDATE UPGRADE EDITION\n");

        /* Create default user 'sovereign' (Syscall 112) */
        __asm__ volatile ("int $3" : : "a"((uint64_t)112), "b"((uint64_t)"sovereign"), "c"((uint64_t)"password"), "d"((uint64_t)&res) : "memory");

        /* Create guest user for default isolation (UID 2000+) */
        __asm__ volatile ("int $3" : : "a"((uint64_t)112), "b"((uint64_t)"guest"), "c"((uint64_t)"guest"), "d"((uint64_t)&res) : "memory");

        /* Initialize Security Policy: UID 2000+ are Guest isolated */
        write_file("DATA:/sys/policy.conf", "GUEST_MIN_UID=2000\nWRITE_PROTECT=/sys,/bin\n");

        /* Write persistent install marker */
        const char* metadata = "{\"version\":\"1.0\",\"status\":\"success\",\"build\":\"23:00\",\"checksum\":\"DEADBEEF\"}\n";
        write_file("DATA:/sys/.installed", metadata);

        /* Redundant Signed Marker at Fixed LBA (LBA 1 of the partition) */
        /* Note: Syscall 120 is high-level VFS write. We need a low-level block write for the marker. */
        /* For this edition, we use a file-based marker as the primary, and log verification. */

        print("[CARGO] Verifying installation marker...\n");
        vfs_handle_t vh = open_file("DATA:/sys/.installed", "r");
        if (vh) {
            char verify_buf[128];
            int br = read_file(vh, verify_buf, 127);
            verify_buf[br] = '\0';
            close_file(vh);
            print("[CARGO] Marker reread: "); print(verify_buf);
        }

        print("[CARGO] Installation payload delivered to SATA HDD.\n");
    }

    gui_draw_text(&fb, px + 350, py + 370, "[ PRESS ENTER TO REBOOT ]", COLOR_PRIMARY);

    char dummy[16];
    rsl_input("", dummy);

    for (;;) {
        __asm__ volatile ("int $0x81");
    }
}
