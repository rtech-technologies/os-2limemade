#include "task.h"
#include <include/rsl.h>
#include <include/panic.h>
#include <stddef.h>

void vga_print(const char* fmt, ...);
void forensic_panic(const char* message, void* state);
void pit_wait_ms(uint32_t ms);

void idle_task(void) {
    while (1) {
        /* Sovereign Idle: Yield to allow other tasks to run */
        sys_yield();
        __asm__ volatile ("pause");
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

static bool kernel_scanning = true;

void tasking_set_scanning(bool scanning) {
    kernel_scanning = scanning;
}

bool tasking_is_scanning(void) {
    return kernel_scanning;
}

void (*pending_shell_entry)(void) = NULL;

void tasking_create_kernel_thread(void (*entry)(void), const char* name) {
    (void)name;
    pending_shell_entry = entry;
}

void tasking_create_process(const char* name) {
    (void)name;
}

void print_service_task(void);

void tasking_init(void) {
    /* Register Idle Task in Slab 0 */
    register_task(idle_task, 0);

    /* Register System Maintenance Task in Slab 1 */
    register_task(system_task, 1);

    /* Register Print Service Task in Slab 1 (Shared with System) */
    register_task(print_service_task, 1);

    /* Register Shell Task in Slab 2 */
    if (pending_shell_entry) {
        register_task(pending_shell_entry, 2);
    }

    /* Context Guard: Verify that tasks were registered correctly */
    extern int get_task_count(void);
    PANIC_ON(get_task_count() < 2, "MULTITASKING_INIT: INSUFFICIENT SYSTEM TASKS");

    unice64_scheduler_init();
    vga_print("[UNICE64] Multitasking initialized.\n");
}
