#include <kernel/libs/services.h>
#include <kernel/libs/vdisk.h>
#include <kernel/libs/pci.h>
#include <include/rsl.h>
#include <include/vfs.h>
#include <limine.h>
#include <stddef.h>

int is_sovereign_disk(int disk_id);
void serial_write_str(const char* s);
void shell_main(void);

#include <kernel/libs/fatfs/ff.h>
void forensic_panic(const char* message, void* state);

void gdt_init(void);
void pmm_init(void);

/* The Ritual: Entry Point */
void _start(void) {
    /* Sovereign Silicon Foundation */
    gdt_init();
    pmm_init();

    /* Initialize Hardware and Core Memory */
    dispatch_event(EVENT_INIT);

    /* Start the Shell and Main System Logic */
    serial_write_str("[EVENT] Entering EVENT_MAIN...\n");

    static FATFS boot_fs;
    bool mount_success = false;

    /* 1. Sovereign Discovery: Scan ALL registered hardware for a bootable volume */
    void ahci_hardware_audit(int p);
    void vga_print(const char* fmt, ...);

    /* First, ensure all AHCI ports are audited and linked */
    for (int p = 0; p < 32; p++) {
        ahci_hardware_audit(p);
    }

    int boot_drive = -1;
    int install_drive = -1;

    int hw_count = get_hw_disk_count();
    vga_print("[BOOT] Scanning %d detected hardware volumes...\n", hw_count);

    for (int i = 0; i < hw_count; i++) {
        uint8_t sector[512];
        if (disk_read(i, sector, 0, 1) == RES_OK) {
            uint32_t sig = *(uint32_t*)sector;
            if (sig == 0xEFBEADDE) {
                if (vdisk_is_atapi(i)) {
                    vga_print("[BOOT] Sovereign Installation Media found (Drive %d).\n", i);
                    install_drive = i;
                } else {
                    vga_print("[BOOT] Sovereign HDD found (Drive %d).\n", i);
                    boot_drive = i;
                    break;
                }
            } else {
                if (!vdisk_is_atapi(i)) {
                    if (sig == 0) {
                        vga_print("[BOOT] Drive %d is empty. Candidate for installation.\n", i);
                    } else {
                        vga_print("[BOOT] Drive %d has unknown signature 0x%x.\n", i, sig);
                    }
                }
            }
        }
    }

    /* Fallback to install drive if no HDD boot found */
    if (boot_drive == -1 && install_drive != -1) {
        vga_print("[BOOT] Starting system from Installation Media (Drive %d).\n", install_drive);
        boot_drive = install_drive;
    }

    if (boot_drive == -1) {
        set_color(YELLOW, BLACK);
        print("\n[BOOT] NO SOVEREIGN DISK FOUND.\n");
        void* choice = input("Would you like to search for non-FAT disks and install? (y/n): ");
        if (choice && str_match(choice, "y")) {
            for (int i = 0; i < hw_count; i++) {
                if (vdisk_is_atapi(i)) continue;
                uint8_t sector[512];
                if (disk_read(i, sector, 0, 1) == RES_OK) {
                    uint32_t sig = *(uint32_t*)sector;
                    if (sig == 0 || sig != 0xEFBEADDE) {
                        vga_print("OSx2: Installing to Drive %d (Signature: 0x%x)...\n", i, sig);
                        uint8_t stamp[512] = {0};
                        stamp[0] = 0xEF; stamp[1] = 0xBE; stamp[2] = 0xAD; stamp[3] = 0xDE;
                        disk_write(i, stamp, 0, 1);

                        if (f_fdisk(i) == FR_OK) {
                            if (f_mkfs(i) == FR_OK) {
                                vga_print("OSx2: Installation Complete on Drive %d.\n", i);
                                boot_drive = i;
                                break;
                            }
                        }
                    }
                }
            }
            release(choice);
        } else if (choice) {
            release(choice);
        }
    }

    if (boot_drive != -1) {
        for (int retry = 0; retry < 3; retry++) {
            if (f_mount(&boot_fs, boot_drive) == FR_OK) {
                /* Register the boot volume with VFS as "BOOT" */
                void internal_fs_ls(void* path);
                void internal_fs_cat(void* path);
                void internal_fs_write(void* path, void* content);
                void internal_fs_mkdir(void* path);
                void internal_fs_rmdir(void* path);
                bool internal_fs_exists(void* path);

                vfs_node_t boot_node = {
                    .ls = internal_fs_ls,
                    .cat = internal_fs_cat,
                    .write = internal_fs_write,
                    .mkdir = internal_fs_mkdir,
                    .rmdir = internal_fs_rmdir,
                    .exists = internal_fs_exists
                };
                /* strcpy-like hack for name */
                const char* bname = "BOOT"; int bk = 0;
                while(bname[bk]) { boot_node.name[bk] = bname[bk]; bk++; } boot_node.name[bk] = '\0';

                vfs_register_node(boot_node);

                void vdisk_connect(int hw_id);
                vdisk_connect(boot_drive);
                mount_success = true;
                vga_print("[FS] Sovereign Volume (Drive %d) Mounted as BOOT.\n", boot_drive);
                break;
            }
            vga_print("[FS] Mount failed on Drive %d, retry %d...\n", boot_drive, retry + 1);
        }
    }

    if (!mount_success) {
        set_color(LIGHT_RED, BLACK);
        print("\n[CRITICAL] SYSTEM CANNOT FIND BOOT DISK.\n");
        print("[CRITICAL] ENTERING SAFE MODE.\n");
    }

    dispatch_event(EVENT_MAIN);

    /* Automated Sovereignty: Try to execute BOOT.RSL */
    void rsl_execute_stream(const char* path);
    if (mount_success) {
        rsl_execute_stream("BOOT:/BOOT.RSL");
    }

    /* Launch the RSL Shell */
    shell_main();

    /* Cleanup and Shutdown */
    dispatch_event(EVENT_CLEANUP);
    dispatch_event(EVENT_EXIT);

    /* Hang if we ever return */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
