#include "task.h"
#include <include/rsl.h>
#include <include/panic.h>
#include <stddef.h>

void vga_print(const char* fmt, ...);
void forensic_panic(const char* message, void* state);
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

void system_task(void) {
    vga_print("[UNICE64] System Maintenance Task Active.\n");
    while (1) {
        /* System Maintenance: Voluntary handover */
        sys_yield();
    }
}

void tasking_create_process(const char* name) {
    /* Hard-coded linear task registration for boot stability */
    (void)name;
}

void tasking_init(void) {
    /* Register Idle Task in Slab 0 */
    register_task(idle_task, 0);

    /* Register System Maintenance Task in Slab 1 */
    register_task(system_task, 1);

    /* Register Shell Task in Slab 2 */
    register_task(shell_task, 2);

    /* Context Guard: Verify that tasks were registered correctly */
    extern int get_task_count(void);
    PANIC_ON(get_task_count() < 2, "MULTITASKING_INIT: INSUFFICIENT SYSTEM TASKS");

    unice64_scheduler_init();
    vga_print("[UNICE64] Multitasking initialized.\n");
}
