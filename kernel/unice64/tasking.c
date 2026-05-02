#include "task.h"
#include <include/rsl.h>
#include <stddef.h>

void vga_print(const char* fmt, ...);
void quartermaster_panic(const char* message, void* state);
void pit_wait_ms(uint32_t ms);

void idle_task(void) {
    while (1) {
        /* Low power state */
        sys_yield();
        __asm__ volatile ("hlt");
    }
}

void shell_main(void);

void task_shell(void) {
    vga_print("[UNICE64] Shell Task Started.\n");
    shell_main();
    /* If shell exits, go into infinite sleep */
    while (1) {
        sys_yield();
        __asm__ volatile ("pause");
    }
}

void system_task(void) {
    vga_print("[UNICE64] System Maintenance Task Active.\n");
    static uint64_t last_audit = 0;
    extern uint64_t get_system_ticks(void);

    while (1) {
        uint64_t now = get_system_ticks();
        /* Throttle hardware audits to every 5 seconds (5000 ticks) */
        if (now - last_audit >= 5000) {
            last_audit = now;
            /* System Maintenance: Check AHCI Port 0 for Sovereign connectivity */
            void ahci_hardware_audit(int p);
            ahci_hardware_audit(0);
        }

        /* Voluntary handover */
        sys_yield();
    }
}

void tasking_init(void) {
    unice64_scheduler_init();

    /* Register Idle Task in Slab 0 */
    register_task(idle_task, 0);

    /* Register Shell Task in Slab 1 */
    register_task(task_shell, 1);

    /* Register System Maintenance Task in Slab 2 */
    register_task(system_task, 2);

    /* Context Guard: Verify that tasks were registered correctly */
    /* (In this architecture, register_task increments task_count) */
    extern int get_task_count(void);
    if (get_task_count() < 3) {
        quartermaster_panic("RTECH: INSUFFICIENT RAM FOR MULTITASKING INITIALIZATION", NULL);
    }

    vga_print("[UNICE64] Multitasking initialized (3 tasks).\n");
}

#include <limine.h>
struct limine_module_response* get_modules(void);

void tasking_spawn_module(int module_index, uint32_t slab_id) {
    struct limine_module_response* resp = get_modules();
    if (!resp || (uint64_t)module_index >= resp->module_count) return;

    struct limine_file* mod = resp->modules[module_index];
    if (!mod->address) return;

    /* stand-alone binaries start at the beginning of the module */
    vga_print("[UNICE64] Spawning module %d as task...\n", module_index);
    register_task((void (*)(void))mod->address, slab_id);
}
