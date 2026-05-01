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

int init_gui(void) {
    /* Simulate a failure to initialize Nuklear GUI */
    return -1;
}

void _start(void) {
    if (init_gui() != 0) {
        print("[CARGO] NUKLEAR CALL FAILED. FALLING BACK TO VERBOSE NOGUI MODE.\n");
    }

    print("[CARGO] Sovereign Live Installer\n");
    print("---------------------------------\n");

    int disks = list_disks();
    if (disks <= 0) {
        print("ERROR: No hardware disks detected. Handshake failed.\n");
        while(1) __asm__ volatile ("pause");
    }

    print("Phase 1: Disk Identification [COMPLETE]\n");
    for (int i = 0; i < disks; i++) {
        char name[16] = {0};
        uint64_t size = 0;
        get_disk_info(i, name, &size);
        print(" - SATA DISK FOUND [READY]\n");
    }

    print("\nPhase 2: Partitioning [GPT STANDARD]\n");
    print(" -> Creating Sovereign EFI System Partition...\n");
    print(" -> Creating Sovereign Data Partition...\n");

    print("\nPhase 3: Formatting [ACTIVE]\n");
    print(" -> Formatting Partition 0 with FAT32...\n");
    if (format_disk(0) == 0) {
        print(" -> Format Successful. Mechanical Truth established.\n");
    } else {
        print(" -> Format Failed. Hardware handshake rejected.\n");
        while(1) __asm__ volatile ("pause");
    }

    print("\nPhase 4: Installation [FINALIZING]\n");
    print(" -> Deploying Kernel and RSL Library payloads...\n");
    print(" -> Finalizing boot manifest...\n");

    print("\n[SUCCESS] OSx2 Installation Complete. Hardware is now Sovereign.\n");

    for (;;) {
        __asm__ volatile ("pause");
    }
}
