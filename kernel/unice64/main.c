#include <ksdk/core/ksdk.h>
#include <include/rsl.h>
#include <kernel/libs/storage/fatfs/ff.h>

void gdt_init(void);
void pmm_init(void);
void idt_init(void);
void apic_init(void);
void apic_timer_init(uint32_t count);
void tasking_init(void);
void slab_init(void);

__attribute__((used, section(".bss"), aligned(16)))
static uint8_t kernel_stack[32768];

void _start(void) {
    __asm__ volatile ("mov %0, %%rsp\nadd $32760, %%rsp" : : "r" (kernel_stack) : "memory");
    __asm__ volatile ("cli");
    gdt_init(); pmm_init(); slab_init(); idt_init();
    dispatch_event(EVENT_INIT);
    __asm__ volatile ("sti");

    serial_write_str("\n[ RTECH SOVEREIGN KERNEL ]\n");
    serial_write_str("[ BUILD 23:00 - MECHANICAL TRUTH ]\n\n");

    static FATFS boot_fs;
    bool mount_success = false;
    int boot_drive = -1;
    int hw_count = get_hw_disk_count();
    vga_print("[BOOT] Scanning %d detected hardware volumes...\n", hw_count);

    for (int i = 0; i < hw_count; i++) {
        if (vdisk_is_atapi(i)) continue;
        if (f_mount(&boot_fs, i) == FR_OK) {
            vga_print("[BOOT] Sovereign HDD Online.\n");
            boot_drive = i; mount_success = true; break;
        }
    }

    if (!mount_success) {
        for (int i = 0; i < hw_count; i++) {
            if (!vdisk_is_atapi(i)) continue;
            if (f_mount(&boot_fs, i) == FR_OK) {
                vga_print("[BOOT] Falling back to Ramdisk/CDROM (Drive %d).\n", i);
                boot_drive = i; mount_success = true; break;
            }
        }
    }

    if (mount_success) {
        void internal_fs_ls(void* path, void* priv);
        void internal_fs_cat(void* path, void* priv);
        void internal_fs_write(void* path, void* content, void* priv);
        void internal_fs_mkdir(void* path, void* priv);
        void internal_fs_rmdir(void* path, void* priv);
        bool internal_fs_exists(void* path, void* priv);

        vfs_node_t boot_node = { .private_data = &boot_fs, .ls = internal_fs_ls, .cat = internal_fs_cat, .write = internal_fs_write, .mkdir = internal_fs_mkdir, .rmdir = internal_fs_rmdir, .exists = internal_fs_exists };
        const char* bname = vdisk_is_atapi(boot_drive) ? "INITRD" : "BOOT";
        int bk = 0; while(bname[bk]) { boot_node.name[bk] = bname[bk]; bk++; } boot_node.name[bk] = '\0';
        vfs_register_node(boot_node);
        vga_print("[FS] Sovereign Volume Mounted as %s.\n", bname);
        vfs_set_safe_mode(false);
    } else {
        vfs_set_safe_mode(true);
    }

    dispatch_event(EVENT_MAIN);
    tasking_init();
    apic_init();
    apic_timer_init(1000000);

    vga_print("[UNICE64] Kernel handover to Scheduler.\n");
    dispatch_event(EVENT_CLEANUP);
    dispatch_event(EVENT_EXIT);

    for (;;) __asm__ volatile ("hlt");
}
