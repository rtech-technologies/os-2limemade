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

    /* Initialize Hardware and Core Memory */
    dispatch_event(EVENT_INIT);
    __asm__ volatile ("sti");

    /* Start the Shell and Main System Logic */
    serial_write_str("[EVENT] Entering EVENT_MAIN...\n");

    static FATFS boot_fs;
    int boot_drive = -1;
    bool mount_success = false;

    /* 1. Sovereign Discovery: Unified Volume Handshake */
    int hw_count = get_hw_disk_count();
    vga_print("[BOOT] Scanning %d detected hardware volumes...\n", hw_count);

    for (int i = 0; hw_count > 0 && i < hw_count; i++) {
        vga_print("[BOOT] Checking Volume %d...\n", i);
        if (f_mount(&boot_fs, i) == FR_OK) {
            vga_print("[FS] Sovereign FAT32 Detected on Volume %d.\n", i);
            boot_drive = i;
            mount_success = true;
            break; /* Primary found, stop scan */
        }
    }

    if (!mount_success) {
        set_color(YELLOW, BLACK);
        vga_print("\n[BOOT] NO SOVEREIGN DISK FOUND.\n");
        vga_print("1. Enter Safe Mode (Ramdisk Only)\n");
        vga_print("2. Choose disk to Format and Install\n");

        while(1) {
            void* choice = input("Select Option (1/2): ");
            if (choice && str_match(choice, "1")) {
                vga_print("[BOOT] Entering Safe Mode...\n");
                vfs_set_safe_mode(true);
                release(choice);
                break;
            } else if (choice && str_match(choice, "2")) {
                vga_print("\nAvailable Disks:\n");
                for(int k=0; k<hw_count; k++) vga_print("%d. Disk %d\n", k, k);
                void* disk_idx_str = input("Select Disk ID to format: ");
                if (disk_idx_str) {
                    int idx = str_to_cstr(disk_idx_str)[0] - '0';
                    if (idx >= 0 && idx < hw_count) {
                        vga_print("[BOOT] Formatting Disk %d...\n", idx);
                        if (f_mkfs(idx) == FR_OK) {
                            vga_print("[BOOT] Format Complete. Please Restart.\n");
                            while(1) __asm__ volatile ("hlt");
                        } else {
                            vga_print("[ERROR] Format failed.\n");
                        }
                    }
                    release(disk_idx_str);
                }
                release(choice);
            } else if (choice) {
                release(choice);
            }
        }
    } else {
        vfs_set_safe_mode(false);

        /* Register the boot volume with VFS as "BOOT" */
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
        vdisk_connect(boot_drive);
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
