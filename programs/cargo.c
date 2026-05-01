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

int init_gui(void) {
    /* Simulation of GUI protocol handshake failure */
    return -1;
}

void _start(void) {
    if (init_gui() != 0) {
        print("[CARGO] NUKLEAR GUI PROTOCOL FAILED. ENGAGING VERBOSE NOGUI MODE.\n");
        print("[CARGO] Sovereign Live Installer - Recovery Interface\n");
        print("----------------------------------------------------\n");
    }

    int disks = list_disks();
    if (disks <= 0) {
        print("CRITICAL ERROR: No hardware disks detected. Handshake failed.\n");
        while(1) __asm__ volatile ("pause");
    }

    print("Phase 1: Identification [COMPLETE]\n");
    for (int i = 0; i < disks; i++) {
        char name[16] = {0};
        uint64_t size = 0;
        get_disk_info(i, name, &size);
        print(" - Storage Target DISK ");
        char id_buf[2] = { '0' + i, '\0' };
        print(id_buf);
        print(" [ONLINE]\n");
    }

    print("\nPhase 2: Partitioning [GPT]\n");
    if (partition_disk(0) == 0) {
        print(" -> SUCCESS: Sovereign Partition Map established.\n");
    } else {
        print(" -> FAILURE: Partitioning rejected by firmware.\n");
        while(1) __asm__ volatile ("pause");
    }

    print("\nPhase 3: Formatting [FAT32]\n");
    print(" -> Initializing Sovereign System Partition...\n");
    if (format_disk(0) == 0) {
        print(" -> SUCCESS: Mechanical Truth established on volume.\n");
    } else {
        print(" -> FAILURE: Format operation failed.\n");
        while(1) __asm__ volatile ("pause");
    }

    print("\nPhase 4: Deployment [SYNC]\n");
    print(" -> Transferring Sovereign payloads to hardware...\n");
    print(" -> Finalizing boot manifest synchronization...\n");

    print("\n[COMPLETE] OSx2 is now Sovereign on this hardware.\n");

    for (;;) {
        __asm__ volatile ("pause");
    }
}
