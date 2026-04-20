#include "task.h"
#include <include/rsl.h>
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

void tasking_init(void) {
    unice64_scheduler_init();

    /* Register Idle Task in Slab 0 */
    register_task(idle_task, 0);

    /* Register Shell Task in Slab 1 */
    register_task(shell_task, 1);

    /* Register System Maintenance Task in Slab 2 */
    register_task(system_task, 2);

    /* Register USB Sovereign Task in Slab 3 */
    void usb_sovereign_task(void);
    register_task(usb_sovereign_task, 3);

    /* Context Guard: Verify that tasks were registered correctly */
    /* (In this architecture, register_task increments task_count) */
    extern int get_task_count(void);
    if (get_task_count() < 4) {
        forensic_panic("RTECH: INSUFFICIENT RAM FOR MULTITASKING INITIALIZATION", NULL);
    }

    vga_print("[UNICE64] Multitasking initialized (4 tasks).\n");
}
