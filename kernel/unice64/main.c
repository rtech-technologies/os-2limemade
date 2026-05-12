#include <kernel/libs/core/services.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>
#include <include/rsl.h>
#include <include/vfs.h>
#include <limine.h>
#include <stddef.h>

int is_sovereign_disk(int disk_id);
void serial_write_str(const char* s);
void shell_main(void);
void vga_print(const char* fmt, ...);

#include <kernel/libs/storage/fatfs/ff.h>
void forensic_panic(const char* message, void* state);

void gdt_init(void);
void pmm_init(void);
void idt_init(void);
uint64_t get_hhdm_offset(void);
void apic_init(void);
void apic_timer_init(uint32_t count);
void tasking_init(void);
void slab_init(void);

/* 32KB Sovereign Stack */
__attribute__((used, section(".bss"), aligned(16)))
static uint8_t kernel_stack[32768];

uint64_t g_hhdm_offset = 0;

/* The Ritual: Entry Point */
void _start(void) {
    /* Switch to larger stack before anything else */
    __asm__ volatile (
        "mov %0, %%rsp\n"
        "add $32760, %%rsp\n"  /* 16-byte Alignment Trick for x86_64 */
        : : "r" (kernel_stack) : "memory"
    );

    /* Sovereign Silicon Foundation */
    __asm__ volatile ("cli");
    g_hhdm_offset = get_hhdm_offset();

    gdt_init();
    pmm_init();
    slab_init();

    /* Pre-register IDT to catch early faults */
    idt_init();

    /* OSx2 Sovereign Professional Boot */
    extern bool g_vga_silent;
    g_vga_silent = true;
    serial_write_str("\n[ RTECH SOVEREIGN KERNEL ]\n");
    serial_write_str("[ BUILD 23:00 - MECHANICAL TRUTH ]\n\n");

    /* Initialize Hardware and Core Memory */
    dispatch_event(EVENT_INIT);
    __asm__ volatile ("sti");

    void rtech_draw_logo(void);
    void boot_spinner_update(int stage);
    rtech_draw_logo();

    /* Start the Shell and Main System Logic */
    serial_write_str("[EVENT] Entering EVENT_MAIN...\n");

    bool mount_success = false;

    /* 1. Sovereign Discovery: Unified Volume Handshake */
    int hw_count = get_hw_disk_count();
    serial_write_str("[BOOT] Scanning hardware volumes...\n");

    for (int i = 0; i < hw_count; i++) {
        boot_spinner_update(i);
        serial_write_str("[BOOT] Verifying Volume...\n");
        static FATFS tmp;
        if (f_mount(&tmp, i) == FR_OK) {
            serial_write_str("[FS] Volume verified as Sovereign.\n");
            mount_success = true;
        }
    }

    if (!mount_success) {
        set_color(LIGHT_RED, BLACK);
        print("\n[CRITICAL] SYSTEM CANNOT FIND BOOT DISK.\n");
        print("[CRITICAL] ENTERING SAFE MODE.\n");
        vfs_set_safe_mode(true);
    } else {
        vfs_set_safe_mode(false);
    }

    /* Initialize Active-Relay Multitasking */
    void vga_print(const char* fmt, ...);
    bool ahci_is_ready(void);
    if (ahci_is_ready()) {
        vga_print("[INIT] AHCI Polling Success - Handoff to Orchestrator\n");
    } else {
        vga_print("[AHCI] Booting in Degraded Mode...\n");
    }

    dispatch_event(EVENT_MAIN);

    void tasking_create_kernel_thread(void (*entry)(void), const char* name);
    void shell_main(void);
    tasking_create_kernel_thread(shell_main, "shell");
    tasking_init();

    /* Initialize APIC for system_ticks (One-Shot Mode) */
    /* Extremely Fast Clock: 30ms interval (approx) */
    apic_init();
    apic_timer_init(50000);

    /* The main thread becomes an observer or a task. */
    vga_print("[INIT] Handing control to RSL Shell...\n");
    vga_print("[UNICE64] Kernel handover to Scheduler.\n");

    /* Release yield-lock before handover */
    void tasking_set_scanning(bool scanning);
    tasking_set_scanning(false);

    /* Start Scheduling */
    void sys_yield(void);
    sys_yield();

    /* Hang if we ever return */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
