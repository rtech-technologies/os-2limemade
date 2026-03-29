#include <kernel/libs/services.h>
#include <include/rsl.h>
#include <limine.h>
#include <stddef.h>

int is_sovereign_disk(int disk_id);
void serial_write_str(const char* s);
void shell_main(void);

#include <kernel/libs/fatfs/ff.h>
void forensic_panic(const char* message, void* state);

/* The Ritual: Entry Point */
void _start(void) {
    /* Initialize Hardware and Core Memory */
    dispatch_event(EVENT_INIT);

    /* Start the Shell and Main System Logic */
    serial_write_str("[EVENT] Entering EVENT_MAIN...\n");

    FATFS fs;
    bool mount_success = false;

    /* 1. Sovereign Discovery: Scan ALL registered hardware for a bootable volume */
    void ahci_hardware_audit(int p);
    void vga_print(const char* fmt, ...);
    int get_hw_disk_count(void);

    /* First, ensure all AHCI ports are audited and linked */
    for (int p = 0; p < 32; p++) {
        ahci_hardware_audit(p);
    }

    int boot_drive = -1;
    int hw_count = get_hw_disk_count();
    vga_print("[BOOT] Scanning %d detected hardware volumes...\n", hw_count);

    for (int i = 0; i < hw_count; i++) {
        uint8_t sector[512];
        if (disk_read(i, sector, 0, 1) == RES_OK) {
            uint32_t sig = *(uint32_t*)sector;
            if (sig == 0xEFBEADDE) {
                vga_print("[BOOT] Sovereign Volume found on Drive %d.\n", i);
                boot_drive = i;
                break;
            } else if (sig == 0 && boot_drive == -1) {
                vga_print("[BOOT] Drive %d is empty. Candidate for installation.\n", i);
            }
        }
    }

    if (boot_drive == -1) {
        set_color(YELLOW, BLACK);
        print("\n[BOOT] NO SOVEREIGN DISK FOUND.\n");
        void* choice = input("Would you like to search for empty disks and install? (y/n): ");
        if (choice && str_match(choice, "y")) {
            for (int i = 0; i < hw_count; i++) {
                uint8_t sector[512];
                if (disk_read(i, sector, 0, 1) == RES_OK) {
                    if (*(uint32_t*)sector == 0) {
                        vga_print("OSx2: Installing to Drive %d...\n", i);
                        uint8_t stamp[512] = {0};
                        stamp[0] = 0xEF; stamp[1] = 0xBE; stamp[2] = 0xAD; stamp[3] = 0xDE;
                        disk_write(i, stamp, 0, 1);

                        FRESULT f_fdisk(int drive);
                        if (f_fdisk(i) == FR_OK) {
                            char drv_path[16];
                            int k = 0;
                            if (i >= 10) drv_path[k++] = '0' + (i / 10);
                            drv_path[k++] = '0' + (i % 10);
                            drv_path[k++] = ':'; drv_path[k++] = '\0';

                            if (f_mkfs(drv_path, 0, 0) == FR_OK) {
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
        char drv_path[16];
        int k = 0;
        if (boot_drive >= 10) drv_path[k++] = '0' + (boot_drive / 10);
        drv_path[k++] = '0' + (boot_drive % 10);
        drv_path[k++] = ':'; drv_path[k++] = '\0';

        for (int retry = 0; retry < 3; retry++) {
            if (f_mount(&fs, drv_path, 1) == FR_OK) {
                void vdisk_connect(int hw_id);
                vdisk_connect(boot_drive);
                mount_success = true;
                vga_print("[FS] Sovereign Volume %s Mounted.\n", drv_path);
                break;
            }
            vga_print("[FS] Mount failed on %s, retry %d...\n", drv_path, retry + 1);
        }
    }

    if (!mount_success) {
        set_color(LIGHT_RED, BLACK);
        print("\n[CRITICAL] SYSTEM CANNOT FIND BOOT DISK.\n");
        print("[CRITICAL] ENTERING SAFE MODE.\n");
        void enter_safe_mode(void);
        enter_safe_mode();
    }

    dispatch_event(EVENT_MAIN);

    /* Automated Sovereignty: Try to execute BOOT.RSL */
    void rsl_execute_stream(const char* path);
    if (mount_success) {
        rsl_execute_stream("0:/BOOT.RSL");
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
