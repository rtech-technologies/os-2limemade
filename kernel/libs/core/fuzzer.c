#include <stdint.h>
#include <stddef.h>
#include <include/rsl.h>
#include <include/stdlib.h>

void vga_print(const char* fmt, ...);
void sys_yield(void);

static uint32_t seed = 0x12345678;
static uint32_t xorshift32(void) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

void rsl_fuzz_syscalls(void) {
    vga_print("[FUZZ] Starting Syscall Fuzzing Harness...\n");
    for (int i = 0; i < 1000; i++) {
        uint64_t rax = xorshift32() % 500;
        uint64_t rbx = xorshift32();
        uint64_t rcx = xorshift32();
        if (rax == 1 || rax == 2) continue;
        __asm__ volatile (
            "mov %0, %%rax\n"
            "mov %1, %%rbx\n"
            "mov %2, %%rcx\n"
            "int $0x80\n"
            : : "r"(rax), "r"(rbx), "r"(rcx) : "rax", "rbx", "rcx"
        );
    }
    vga_print("[FUZZ] Syscall Fuzzing Complete.\n");
}

void rsl_fuzz_ioctls(void) {
    vga_print("[FUZZ] Starting IOCTL Fuzzing Harness...\n");
    extern void ahci_hardware_audit(int p);
    extern void nvme_reset_controller(void);
    for (int i = 0; i < 10; i++) {
        ahci_hardware_audit(xorshift32() % 32);
        if (xorshift32() % 2) nvme_reset_controller();
    }
    vga_print("[FUZZ] IOCTL Fuzzing Complete.\n");
}

void rsl_stress_test(void) {
    vga_print("[STRESS] Starting DMA/IRQ Stress Suite...\n");
    uint8_t* buffer = (uint8_t*)malloc(32768);
    if (!buffer) return;
    for (int i = 0; i < 50; i++) {
        extern int vdisk_read(int disk_id, uint64_t lba, uint32_t count, void* buffer);
        vdisk_read(0, xorshift32() % 100, 64, buffer);
        sys_yield();
    }
    free(buffer);
    vga_print("[STRESS] Stress Test Complete.\n");
}
