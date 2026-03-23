#include <kernel/libs/services.h>
#include <kernel/libs/fatfs/ff.h>
#include <limine.h>
#include <stddef.h>

int is_sovereign_disk(int disk_id);
void serial_write_str(const char* s);

/* The Ritual: Entry Point */
void _start(void) {
    /* Initialize Hardware and Core Memory */
    dispatch_event(EVENT_INIT);

    /* Start the Shell and Main System Logic */
    serial_write_str("[EVENT] Entering EVENT_MAIN...\n");

    /* Attempt to mount Sovereign Disks */
    static FATFS fs;
    if (is_sovereign_disk(0)) {
        if (f_mount(&fs, "0:", 1) == FR_OK) {
            serial_write_str("[FS] FatFS mounted Sovereign Disk 0.\n");
        } else {
            serial_write_str("[FS] CANNOT FIND DISK autopsy - FatFS mount failed.\n");
        }
    } else {
        serial_write_str("[FS] No Sovereign disk found on ID 0.\n");
    }

    dispatch_event(EVENT_MAIN);

    /* Cleanup and Shutdown */
    dispatch_event(EVENT_CLEANUP);
    dispatch_event(EVENT_EXIT);

    /* Hang if we ever return */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
