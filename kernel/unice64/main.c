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

    /* SSE Initialization */
    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); /* Clear EM bit */
    cr0 |= (1 << 1);  /* Set MP bit */
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    uint64_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);  /* Set OSFXSR bit */
    cr4 |= (1 << 10); /* Set OSXMMEXCPT bit */
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4));

    /* Initialize Timer Hardware BEFORE device discovery */
    apic_init();
    apic_timer_init(50000);

    /* OSx2 Sovereign Welcome */
    serial_write_str("\n[ RTECH SOVEREIGN KERNEL ]\n");
    serial_write_str("[ BUILD 23:00 - MECHANICAL TRUTH ]\n\n");

    /*
     * Opaque Sheep Boot Flow:
     * Check if 'quiet' is passed in the command line from Limine.
     */
    extern bool g_vga_silent;
    struct limine_kernel_file_response* kf_resp = (void*)0;
    extern struct limine_kernel_file_response* get_kernel_file(void);
    kf_resp = get_kernel_file();

    bool quiet_mode = false;
    if (kf_resp && kf_resp->kernel_file && kf_resp->kernel_file->cmdline) {
        const char* cmd = kf_resp->kernel_file->cmdline;
        /* Simple substring check for "quiet" */
        for (int i = 0; cmd[i]; i++) {
            if (cmd[i] == 'q' && cmd[i+1] == 'u' && cmd[i+2] == 'i' && cmd[i+3] == 'e' && cmd[i+4] == 't') {
                quiet_mode = true;
                break;
            }
        }
    }

    if (quiet_mode) {
        g_vga_silent = true;
    } else {
        g_vga_silent = false;
        void vga_print_logo(void);
        vga_print_logo();
    }

    /* Initialize Hardware and Core Memory */
    dispatch_event(EVENT_INIT);
    __asm__ volatile ("sti");

    /* Start the Shell and Main System Logic */
    serial_write_str("[EVENT] Entering EVENT_MAIN...\n");

    bool mount_success = false;
    static FATFS boot_fs;

    /* 1. Sovereign Discovery: Unified Volume Handshake */
    int hw_count = get_hw_disk_count();
    vga_print("[BOOT] Scanning %d detected hardware volumes...\n", hw_count);

    for (int i = 0; i < hw_count; i++) {
        vga_print("[BOOT] Verifying Volume %d...\n", i);
        if (f_mount(&boot_fs, i) == FR_OK) {
            vga_print("[FS] Volume %d verified as Sovereign.\n", i);
            mount_success = true;

            /* Bridge to VFS */
            void internal_fs_ls(void* path, void* priv);
            void internal_fs_cat(void* path, void* priv);
            void internal_fs_write(void* path, void* content, void* priv);
            void internal_fs_mkdir(void* path, void* priv);
            void internal_fs_rmdir(void* path, void* priv);
            bool internal_fs_exists(void* path, void* priv);

            vfs_node_t boot_node = {
                .private_data = &boot_fs,
                .ls = internal_fs_ls,
                .cat = internal_fs_cat,
                .write = internal_fs_write,
                .mkdir = internal_fs_mkdir,
                .rmdir = internal_fs_rmdir,
                .exists = internal_fs_exists
            };
            const char* bname = "BOOT";
            int bk = 0; while(bname[bk]) { boot_node.name[bk] = bname[bk]; bk++; } boot_node.name[bk] = '\0';
            vfs_register_node(boot_node);

            void vdisk_connect(int hw_id);
            vdisk_connect(i);
            break;
        }
    }

    if (!mount_success) {
        vga_print("\n[CRITICAL] SYSTEM CANNOT FIND BOOT DISK.\n");
        vga_print("[CRITICAL] ENTERING SAFE MODE.\n");
        vfs_set_safe_mode(true);
    } else {
        vfs_set_safe_mode(false);
    }

    /* Initialize Active-Relay Multitasking */
    bool ahci_is_ready(void);
    if (ahci_is_ready()) {
        vga_print("[INIT] AHCI Polling Success - Handoff to Orchestrator\n");
    } else {
        vga_print("[AHCI] Booting in Degraded Mode...\n");
    }

    dispatch_event(EVENT_MAIN);

    /* End of boot sequence: Shell starts, so we need VGA output even if was quiet */
    g_vga_silent = false;

    void tasking_create_kernel_thread(void (*entry)(void), const char* name);
    void shell_task(void);
    tasking_create_kernel_thread(shell_task, "shell");
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
