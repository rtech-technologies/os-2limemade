#include "task.h"
#include <include/rsl.h>
#include <stddef.h>

void vga_print(const char* fmt, ...);
void pit_wait_ms(uint32_t ms);

void idle_task(void) {
    while (1) {
        /* Low power state */
        __asm__ volatile ("hlt");
    }
}

void shell_main(void);

void shell_task(void) {
    vga_print("[UNICE64] Shell Task Started.\n");
    shell_main();
    /* If shell exits, go into infinite sleep */
    while (1) { __asm__ volatile ("hlt"); }
}

void tasking_init(void) {
    unice64_scheduler_init();

    /* Register Idle Task in Slab 0 */
    register_task(idle_task, 0);

    /* Register Shell Task in Slab 1 */
    register_task(shell_task, 1);

    vga_print("[UNICE64] Multitasking initialized (2 tasks).\n");
}
