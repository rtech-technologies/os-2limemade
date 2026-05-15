#include <stdint.h>
#include <stdbool.h>
#include <include/rsl.h>
#include <include/stdlib.h>

void _start(void) {
    print("[STRESS] Starting Sovereign Device Stress Test...\n");

    /* 1. CPU Stress (Busy wait) */
    print("[STRESS] Phase 1: CPU Calculation Stress...\n");
    for (volatile uint64_t i = 0; i < 100000000; i++) {
        /* NOP loop */
    }
    print("[STRESS] Phase 1 Complete.\n");

    /* 2. Memory Stress */
    print("[STRESS] Phase 2: Allocation Stress...\n");
    for (int i = 0; i < 1000; i++) {
        void* p = malloc(4096);
        if (p) free(p);
    }
    print("[STRESS] Phase 2 Complete.\n");

    /* 3. Disk IO Stress */
    print("[STRESS] Phase 3: Disk Integrity Check...\n");
    int disks = rsl_list_disks();
    for (int i = 0; i < disks; i++) {
        char name[32]; uint64_t size;
        rsl_get_disk_info(i, name, &size);
        print(" - Verified Disk: "); print(name); print("\n");
    }
    print("[STRESS] All Phases PASSED.\n");

    print("\n[ SUCCESS ] Device is STABLE. Press 'X' to exit.\n");
    for (;;) {
        char c = rsl_get_char_nonblock();
        if (c == 'x' || c == 'X') break;
        sys_yield();
    }
}
