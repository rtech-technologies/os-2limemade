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
void apic_init(void);
void apic_timer_init(uint32_t count);
void tasking_init(void);
void slab_init(void);

/* 32KB Sovereign Stack */
__attribute__((used, section(".bss"), aligned(16)))
static uint8_t kernel_stack[32768];

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
    gdt_init();
    pmm_init();
    slab_init();

    /* Pre-register IDT to catch early faults */
    idt_init();

    /* OSx2 Sovereign Welcome */
    serial_write_str("\n[ RTECH SOVEREIGN KERNEL ]\n");
    serial_write_str("[ BUILD 23:00 - MECHANICAL TRUTH ]\n\n");

    /* Initialize Hardware and Core Memory */
    dispatch_event(EVENT_INIT);
    __asm__ volatile ("sti");

    /* Start the Shell and Main System Logic */
    serial_write_str("[EVENT] Entering EVENT_MAIN...\n");

    bool mount_success = false;

    /* 1. Sovereign Discovery: Unified Volume Handshake */
    int hw_count = get_hw_disk_count();
    vga_print("[BOOT] Scanning %d detected hardware volumes...\n", hw_count);

    static FATFS disk_fses[16];
    int disk_idx = 0;

    for (int i = 0; i < hw_count; i++) {
        vga_print("[BOOT] Attempting Mount (Drive %d)...\n", i);
        if (f_mount(&disk_fses[i], i) == FR_OK) {
            void internal_fs_ls(void* path, void* priv);
            void internal_fs_cat(void* path, void* priv);
            void internal_fs_write(void* path, void* content, void* priv);
            void internal_fs_mkdir(void* path, void* priv);
            void internal_fs_rmdir(void* path, void* priv);
            bool internal_fs_exists(void* path, void* priv);

            vfs_node_t node = {
                .private_data = &disk_fses[i],
                .ls = internal_fs_ls,
                .cat = internal_fs_cat,
                .write = internal_fs_write,
                .mkdir = internal_fs_mkdir,
                .rmdir = internal_fs_rmdir,
                .exists = internal_fs_exists
            };

            if (!mount_success) {
                const char* name = "BOOT";
                int k = 0; while(name[k]) { node.name[k] = name[k]; k++; } node.name[k] = '\0';
                vfs_register_node(node);
                vga_print("[FS] Drive %d Registered as Primary BOOT:/ Volume.\n", i);
                mount_success = true;
            } else {
                node.name[0] = 'D'; node.name[1] = 'I'; node.name[2] = 'S'; node.name[3] = 'K';
                node.name[4] = '0' + disk_idx; node.name[5] = '\0';
                vfs_register_node(node);
                vga_print("[FS] Drive %d Registered as %s:/.\n", i, node.name);
                disk_idx++;
            }
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

    void tasking_create_process(const char* name);
    tasking_create_process("shell");
    tasking_init();

    /* Initialize APIC for system_ticks (One-Shot Mode) */
    apic_init();
    apic_timer_init(1000000);

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
