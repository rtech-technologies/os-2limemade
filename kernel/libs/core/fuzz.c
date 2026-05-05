#include "fuzz.h"
#include <kernel/libs/core/services.h>
#include <stddef.h>

void vga_print(const char* fmt, ...);
void rsl_syscall_handler(uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, uint64_t rsi);

static uint32_t seed = 0xDEADC0DE;
static uint32_t rand(void) {
    seed = seed * 1103515245 + 12345;
    return (seed / 65536) % 32768;
}

void syscall_fuzz_harness(void) {
    vga_print("[FUZZ] Starting Syscall Fuzzing...\n");
    for (int i = 0; i < 100; i++) {
        uint64_t rax = rand() % 500;
        uint64_t rbx = rand();
        uint64_t rcx = rand();
        uint64_t rdx = rand();
        uint64_t rsi = rand();
        /* Use a 'safe' address for pointers that might be dereferenced */
        rsl_syscall_handler(rax, rbx, rcx, rdx, rsi);
    }
    vga_print("[FUZZ] Syscall Fuzzing Complete.\n");
}

void driver_ioctl_fuzz(void) {
    vga_print("[FUZZ] Starting NVMe IOCTL Stress Test...\n");
    int nvme_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer);
    /* Fuzz NVMe read with random LBAs and large counts */
    for (int i = 0; i < 50; i++) {
        uint64_t lba = rand() % 0x100000;
        uint32_t count = (rand() % 128) + 1;
        void* buf = (void*)((uintptr_t)0x1000000 + (rand() % 0x100000));
        nvme_read_sectors(NULL, lba, count, buf);
    }
    vga_print("[FUZZ] NVMe Stress Test Complete.\n");
}
