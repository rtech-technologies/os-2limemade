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
    if (f_mount(&fs, "0:", 1) != FR_OK) {
        set_color(YELLOW, BLACK);
        print("\n[FS] WARNING: Disk 0 is not a Sovereign FAT32 volume.\n");
        void* choice = input("Would you like to format Disk 0 now? (y/n): ");
        if (choice && str_match(choice, "y")) {
            if (f_mkfs("0:", 0, 0) == FR_OK) {
                print("[FS] Disk formatted. Retrying mount...\n");
                if (f_mount(&fs, "0:", 1) != FR_OK) {
                    forensic_panic("FORMAT SUCCESS BUT MOUNT FAILED", NULL);
                }
            } else {
                forensic_panic("DISK FORMAT FAILED", NULL);
            }
            release(choice);
        } else {
            if (choice) release(choice);
            forensic_panic("CANNOT PROCEED WITHOUT SOVEREIGN FS", NULL);
        }
    }
    serial_write_str("[FS] Disk 0 Mounted successfully via AHCI.\n");

    dispatch_event(EVENT_MAIN);

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
