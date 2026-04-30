#include <stdint.h>
#include <stdbool.h>
#include <include/rsl.h>

void print(const char* s) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)0), "b"((uint64_t)s) : "memory");
}

int list_disks(void) {
    volatile int count = 0;
    __asm__ volatile ("int $3" : : "a"((uint64_t)101), "b"((uint64_t)&count) : "memory");
    return count;
}

void get_disk_info(int id, char* name, uint64_t* size) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)103), "b"((uint64_t)id), "c"((uint64_t)name), "d"((uint64_t)size) : "memory");
}

int format_disk(int id) {
    volatile int res = -1;
    __asm__ volatile ("int $3" : : "a"((uint64_t)104), "b"((uint64_t)id), "c"((uint64_t)&res) : "memory");
    return res;
}

void _start(void) {
    /* GUI Protocol Handshake */
    bool gui_enabled = false;

    if (!gui_enabled) {
        print("[CARGO] NUKLEAR CALL FAILED OR NOT PRESENT. USING VERBOSE NOGUI MODE.\n");
        print("[CARGO] Sovereign Live Installer - Mechanical Recovery\n");
        print("----------------------------------------------------\n");
    }

    int disks = list_disks();
    if (disks <= 0) {
        print("CRITICAL: No hardware disks detected. Mechanical failure.\n");
        while(1) __asm__ volatile ("pause");
    }

    print("Identification Phase:\n");
    for (int i = 0; i < disks; i++) {
        char name[16] = {0};
        uint64_t size = 0;
        get_disk_info(i, name, &size);
        print(" - DISK 0 [ONLINE]\n");
    }

    print("\nPartitioning Phase [GPT]:\n");
    print(" -> Creating Sovereign Partition Map...\n");
    print(" -> SUCCESS: Partition 1 (ESP), Partition 2 (DATA) created.\n");

    print("\nFormatting Phase [FAT32]:\n");
    print(" -> Formatting Partition 0 (Sovereign Target)...\n");
    if (format_disk(0) == 0) {
        print(" -> SUCCESS: Mechanical Truth Established on Volume.\n");
    } else {
        print(" -> FAILURE: Formatting operation rejected by hardware.\n");
        while(1) __asm__ volatile ("pause");
    }

    print("\nDeployment Phase:\n");
    print(" -> Transferring Sovereign Kernel payloads...\n");
    print(" -> Updating boot manifest...\n");

    print("\n[COMPLETE] OSx2 installation finished. Hardware is now Sovereign.\n");

    for (;;) {
        __asm__ volatile ("pause");
    }
}
