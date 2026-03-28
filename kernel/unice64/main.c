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

    /* 1. Sovereign Discovery: Check for Empty Disk or Existing Signature */
    uint8_t sector[512];
    if (disk_read(0, sector, 0, 1) == RES_OK) {
        uint32_t sig = *(uint32_t*)sector;
        if (sig == 0) {
            set_color(LIGHT_CYAN, BLACK);
            print("OSx2: Empty Disk Detected. Claiming Sovereignty...\n");

            /* Stamp the disk at LBA 0 */
            uint8_t stamp[512] = {0};
            stamp[0] = 0xEF; stamp[1] = 0xBE; stamp[2] = 0xAD; stamp[3] = 0xDE;
            disk_write(0, stamp, 0, 1);

            print("OSx2: Building Partition Map and Formatting FAT32...\n");
            FRESULT f_fdisk(int drive);
            if (f_fdisk(0) == FR_OK) {
                if (f_mkfs("0:", 0, 0) == FR_OK) {
                    print("OSx2: Installation Complete. 0:/0/ is Online.\n");
                }
            }
        }
    }

    for (int retry = 0; retry < 3; retry++) {
        if (f_mount(&fs, "0:", 1) == FR_OK) {
            void vdisk_connect(int hw_id);
            vdisk_connect(0);
            mount_success = true;
            break;
        }
        serial_write_str("[FS] Mount attempt failed, retrying...\n");
    }

    if (!mount_success) {
        set_color(YELLOW, BLACK);
        print("\n[FS] WARNING: Disk 0 is not a Sovereign FAT32 volume.\n");
        void* choice = input("Would you like to format Disk 0 now? (y/n): ");
        if (choice && str_match(choice, "y")) {
            if (f_mkfs("0:", 0, 0) == FR_OK) {
                print("[FS] Disk formatted. Retrying mount...\n");
                if (f_mount(&fs, "0:", 1) == FR_OK) {
                    void vdisk_connect(int hw_id);
                    vdisk_connect(0);
                } else {
                    print("[FS] Mount failed. Entering Safe Mode.\n");
                    void enter_safe_mode(void);
                    enter_safe_mode();
                }
            } else {
                print("[FS] Format failed. Entering Safe Mode.\n");
                void enter_safe_mode(void);
                enter_safe_mode();
            }
            release(choice);
        } else {
            if (choice) release(choice);
            print("[FS] Skipping format. Entering Safe Mode.\n");
            void enter_safe_mode(void);
            enter_safe_mode();
        }
    }
    serial_write_str("[FS] Disk 0 Mounted successfully via AHCI.\n");

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
