#include <kernel/libs/core/services.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>
#include <include/rsl.h>
#include <include/vfs.h>
#include <kernel/unice64/task.h>
#include <limine.h>
#include <stddef.h>
#include <kernel/libs/storage/fatfs/ff.h>

void gdt_init(void);
void pmm_init(void);
void idt_init(void);
void apic_init(void);
void apic_timer_init(uint32_t count);
void tasking_init(void);
void slab_init(void);
void serial_write_str(const char* s);
void vga_print(const char* fmt, ...);
void _sse_init(void);

__attribute__((used, section(".bss"), aligned(16)))
static uint8_t kernel_stack[32768];

void _start(void) {
    __asm__ volatile (
        "mov %0, %%rsp\n"
        "add $32760, %%rsp\n"
        : : "r" (kernel_stack) : "memory"
    );

    __asm__ volatile ("cli");
    _sse_init();
    gdt_init();
    pmm_init();
    slab_init();
    unice64_scheduler_init();
    idt_init();

    serial_write_str("\n[ RTECH SOVEREIGN KERNEL ]\n");
    serial_write_str("[ BUILD 23:00 - MECHANICAL TRUTH ]\n\n");

    dispatch_event(EVENT_INIT);
    pci_scan_bus();

    __asm__ volatile ("sti");

    static FATFS boot_fs;
    bool mount_success = false;
    int boot_drive = -1;
    int hw_count = get_hw_disk_count();

    for (int i = 0; i < hw_count; i++) {
        if (vdisk_is_atapi(i)) continue;
        if (f_mount(&boot_fs, i) == FR_OK) {
            boot_drive = i;
            mount_success = true;
            break;
        }
    }

    if (!mount_success) {
        for (int i = 0; i < hw_count; i++) {
            if (!vdisk_is_atapi(i)) continue;
            if (f_mount(&boot_fs, i) == FR_OK) {
                boot_drive = i;
                mount_success = true;
                break;
            }
        }
    }

    if (mount_success) {
        extern vfs_handle_internal_t* internal_fs_open(void* path, const char* mode, void* priv);
        extern void internal_fs_ls(void* path, void* priv);
        extern void internal_fs_cat(void* path, void* priv);
        extern void internal_fs_write(void* path, void* content, void* priv);
        extern void internal_fs_mkdir(void* path, void* priv);
        extern void internal_fs_rmdir(void* path, void* priv);
        extern bool internal_fs_exists(void* path, void* priv);

        vfs_node_t boot_node = {
            .private_data = &boot_fs,
            .ls = internal_fs_ls,
            .cat = internal_fs_cat,
            .write = internal_fs_write,
            .mkdir = internal_fs_mkdir,
            .rmdir = internal_fs_rmdir,
            .exists = internal_fs_exists,
            .open = internal_fs_open
        };
        const char* bname = vdisk_is_atapi(boot_drive) ? "INITRD" : "BOOT";
        int bk = 0; while(bname[bk]) { boot_node.name[bk] = bname[bk]; bk++; } boot_node.name[bk] = '\0';
        vfs_register_node(boot_node);
        vga_print("[FS] Sovereign Volume (Drive %d) Mounted as %s.\n", boot_drive, bname);
    }

    vfs_set_safe_mode(!mount_success);
    dispatch_event(EVENT_MAIN);

    tasking_init();
    apic_init();
    apic_timer_init(1000000);

    /* SYSTEM User handling logic would go here, explicitly creating a SYSTEM task if needed */
    /* Handover */
    vga_print("[UNICE64] Kernel handover to Scheduler.\n");
    dispatch_event(EVENT_CLEANUP);
    for (;;) { sys_yield(); __asm__ volatile ("pause"); }
}
