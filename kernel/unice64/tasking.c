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
        /* System Maintenance: Check AHCI Port 0 for Sovereign connectivity */
        void ahci_hardware_audit(int p);
        ahci_hardware_audit(0);

        /* Voluntary handover */
        sys_yield();
    }
}

void tasking_create_process(const char* name) {
    bool str_match(void* str, const char* pattern);
    void* str_create(const char* cstr);
    void release(void* ptr);

    void* s = str_create(name);
    if (str_match(s, "shell")) {
        register_task(shell_task, 1);
    }
    release(s);
}

void tasking_init(void) {
    unice64_scheduler_init();

    /* Register Idle Task in Slab 0 */
    register_task(idle_task, 0);

    /* Register System Maintenance Task in Slab 2 */
    register_task(system_task, 2);

    /* Context Guard: Verify that tasks were registered correctly */
    extern int get_task_count(void);
    PANIC_ON(get_task_count() < 2, "MULTITASKING_INIT: INSUFFICIENT SYSTEM TASKS");

    vga_print("[UNICE64] Multitasking initialized.\n");
}
