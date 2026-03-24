#include <kernel/libs/services.h>
#include <limine.h>
#include <stddef.h>

int is_sovereign_disk(int disk_id);
void serial_write_str(const char* s);
void shell_main(void);

/* The Ritual: Entry Point */
void _start(void) {
    /* Initialize Hardware and Core Memory */
    dispatch_event(EVENT_INIT);

    /* Start the Shell and Main System Logic */
    serial_write_str("[EVENT] Entering EVENT_MAIN...\n");

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
