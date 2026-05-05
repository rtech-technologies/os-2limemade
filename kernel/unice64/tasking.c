#include "task.h"
#include <include/rsl.h>
#include <include/limine.h>
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

void kernel_fallback_shell(void);
struct limine_module_response* get_modules(void);

void task_shell(void) {
    vga_print("[UNICE64] Shell Task Started.\n");

    /* Attempt to spawn standalone shell from module 2 (shell.bin) */
    /* Logistics: Module 0 = Ramdisk, 1 = Cargo, 2 = Shell */
    struct limine_module_response* resp = get_modules();
    if (resp && resp->module_count >= 3) {
        vga_print("[UNICE64] Found shell.bin, spawning...\n");
        void tasking_spawn_module(int module_index, uint32_t slab_id, uint32_t uaid);
        tasking_spawn_module(2, 1, 100);
        /* Standalone shell is now its own task. This wrapper task becomes the fallback monitor. */
    } else {
        vga_print("[WARN] Standalone shell.bin not found. Engaging Fallback...\n");
        kernel_fallback_shell();
    }

    /* If shell exits or fallback is used, go into infinite sleep */
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

void tasking_spawn_module(int module_index, uint32_t slab_id, uint32_t uaid) {
    struct limine_module_response* resp = get_modules();
    if (!resp || (uint64_t)module_index >= resp->module_count) return;

    struct limine_file* mod = resp->modules[module_index];
    if (!mod->address) return;

    /* Quartermaster: Flat Binary RTECH Handshake */
    uint8_t* ptr = (uint8_t*)mod->address;
    void (*entry)(void) = (void (*)(void))mod->address;

    if (ptr[0] == 'R' && ptr[1] == 'T' && ptr[2] == 'E' && ptr[3] == 'C' && ptr[4] == 'H') {
        rtech_header_t* header = (rtech_header_t*)mod->address;
        entry = (void (*)(void))((uintptr_t)mod->address + header->entry_offset);
        vga_print("[UNICE64] RTECH Header valid. Entry offset: 0x%x\n", (uint32_t)header->entry_offset);
    } else {
        vga_print("[WARN] Module %d has no RTECH header. Assuming LBA 0 entry.\n", module_index);
    }

    vga_print("[UNICE64] Spawning module %d (UAID %d) as task...\n", module_index, uaid);
    register_task(entry, slab_id);

    /* Set UAID for the newly created task */
    extern int get_task_count(void);
    extern task_t* get_task_by_idx(int idx);
    int idx = get_task_count() - 1;
    task_t* t = get_task_by_idx(idx);
    if (t) t->uaid = uaid;
}
