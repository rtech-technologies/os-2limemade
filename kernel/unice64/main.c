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

#include <kernel/libs/storage/fatfs/ff.h>
void quartermaster_panic(const char* message, void* state);

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
    serial_write_str("CHECKPOINT 0: Ready for Discovery.\n");

    static FATFS boot_fs;
    bool mount_success = false;

    /* 1. Sovereign Discovery: Scan ALL registered hardware for a bootable volume */
    void vga_print(const char* fmt, ...);

    int boot_drive = -1;
    int hw_count = get_hw_disk_count();
    vga_print("[BOOT] Scanning %d detected hardware volumes...\n", hw_count);

    /* Phase 1: Hardware Mount (SATA first, 50ms Timeout) */
    for (int i = 0; i < hw_count; i++) {
        if (vdisk_is_atapi(i)) continue;

        vga_print("[BOOT] Attempting SATA Mount (Drive %d)...\n", i);
        /* Simple polling mount for timeout logic */
        FRESULT res = f_mount(&boot_fs, i);
        if (res == FR_OK) {
            vga_print("[BOOT] Sovereign HDD Online.\n");
            boot_drive = i;
            mount_success = true;
            break;
        } else {
            vga_print("[BOOT] Drive %d: MOUNT FAILURE or TIMEOUT.\n", i);
        }
    }

    /* Phase 2: INITRD/Fallback (If no functional SATA or mount failure) */
    if (!mount_success) {
        for (int i = 0; i < hw_count; i++) {
            if (!vdisk_is_atapi(i)) continue;
            if (f_mount(&boot_fs, i) == FR_OK) {
                vga_print("[BOOT] Falling back to Ramdisk/CDROM (Drive %d).\n", i);
                boot_drive = i;
                mount_success = true;
                break;
            }
        }
    }

    if (boot_drive == -1) {
        set_color(YELLOW, BLACK);
        print("\n[BOOT] NO SOVEREIGN DISK FOUND.\n");
        /* Quartermaster: Automating Cargo Installer Deployment */
        if (hw_count > 0) {
            vga_print("[SNAP] ENGAGING CARGO INSTALLER...\n");

            /* Find and Execute Installer Payload */
            void rsl_execute_stream(const char* path);
            /* The ramdisk is typically INITRD and is verified in vdisk.c */
            rsl_execute_stream("INITRD:/bin/cargo.bin");
        }
    }

    if (mount_success) {
        /* Register the boot volume with VFS as "BOOT" or "INITRD" */
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
        /* strcpy-like hack for name */
        const char* bname = vdisk_is_atapi(boot_drive) ? "INITRD" : "BOOT";
        int bk = 0;
        while(bname[bk]) { boot_node.name[bk] = bname[bk]; bk++; } boot_node.name[bk] = '\0';

        vfs_register_node(boot_node);

        void vdisk_connect(int hw_id);
        vdisk_connect(boot_drive);
        vga_print("[FS] Sovereign Volume (Drive %d) Mounted as %s.\n", boot_drive, bname);

        /* VFS Bridge: Mount second disk if it exists */
        if (hw_count > 1) {
            static FATFS data_fs;
            int second_drive = (boot_drive == 0) ? 1 : 0;
            if (f_mount(&data_fs, second_drive) == FR_OK) {
                vfs_node_t data_node = {
                    .private_data = &data_fs,
                    .ls = internal_fs_ls,
                    .cat = internal_fs_cat,
                    .write = internal_fs_write,
                    .mkdir = internal_fs_mkdir,
                    .rmdir = internal_fs_rmdir,
                    .exists = internal_fs_exists
                };
                const char* dname = "DISK0";
                int dk = 0; while(dname[dk]) { data_node.name[dk] = dname[dk]; dk++; } data_node.name[dk] = '\0';
                vfs_register_node(data_node);
                vga_print("[FS] SATA HDD (Drive %d) Mounted as DISK0.\n", second_drive);
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

    dispatch_event(EVENT_MAIN);

    /* Initialize Active-Relay Multitasking */
    tasking_init();
    /* Initialize APIC for system_ticks (One-Shot Mode) */
    apic_init();
    apic_timer_init(1000000);

    /* Automated Sovereignty: Try to execute BOOT.RSL */
    void rsl_execute_stream(const char* path);
    if (mount_success) {
        const char* script_path = vdisk_is_atapi(boot_drive) ? "INITRD:/BOOT.RSL" : "BOOT:/BOOT.RSL";
        serial_write_str("CHECKPOINT A: Executing stream...\n");
        rsl_execute_stream(script_path);
        serial_write_str("CHECKPOINT B: Stream finished.\n");
    }

    /* The main thread becomes an observer or a task.
       Actually, tasking_init already registered the shell.
       We should just loop here and let the scheduler take over. */
    vga_print("[UNICE64] Kernel handover to Scheduler.\n");
    dispatch_event(EVENT_CLEANUP);
    dispatch_event(EVENT_EXIT);

    /* Hang if we ever return */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
