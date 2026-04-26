#include "task.h"
#include <include/rsl.h>
#include <include/vfs.h>
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

void usb_main_task(void);
void usb_keyboard_task(void);
void usb_mouse_task(void);
void rtc64_wm_task(void);
void text_editor_task(void);

void shell_main(void);

void task_shell(void) {
    vga_print("[UNICE64] Shell Task Started (Kernel Integrated).\n");
    shell_main();
    /* If shell exits, go into infinite sleep */
    while (1) { sys_yield(); }
}

void sovereign_service_orchestrator(void);

void system_task(void) {
    vga_print("[UNICE64] System Maintenance Task Active.\n");
    while (1) {
        /* Process Hardware/FS requests from other tasks */
        sovereign_service_orchestrator();

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

int tasking_spawn_app(const char* path) {
    vga_print("[UNICE64] Spawning dynamic app: %s\n", path);

    void* pstr = str_create(path);
    vfs_handle_t* h = vfs_open(pstr, "r");
    if (!h) {
        vga_print("[UNICE64] Error: Could not open app binary.\n");
        release(pstr);
        return -1;
    }

    int slab_grab_transient(void);
    int slab_id = slab_grab_transient();
    if (slab_id == -1) {
        vga_print("[UNICE64] Error: No available slabs for app.\n");
        vfs_close(h);
        release(pstr);
        return -1;
    }

    void* slab_get_base(int id);
    uint8_t* code_dest = (uint8_t*)slab_get_base(slab_id);

    /* Load binary into slab */
    int bytes_read = vfs_read(h, code_dest, 1024 * 1024); /* Max 1MB app for now */
    if (bytes_read <= 0) {
        vga_print("[UNICE64] Error: Failed to read app binary.\n");
        void slab_release_transient(int id);
        slab_release_transient(slab_id);
        vfs_close(h);
        release(pstr);
        return -1;
    }

    vfs_close(h);
    release(pstr);

    /* Skip 32-byte RSL header */
    void (*entry_point)(void) = (void (*)(void))(code_dest + 32);

    /* Register loaded code as a transient task */
    int tid = register_transient_task(entry_point, slab_id, 0);
    if (tid != -1) {
        void scheduler_force_task(int task_id);
        scheduler_force_task(tid);
        vga_print("[UNICE64] App %s running in Task %d (Slab %d)\n", path, tid, slab_id);
    } else {
        void slab_release_transient(int id);
        slab_release_transient(slab_id);
    }

    return tid;
}

void print_service_task(void);

void tasking_init(void) {
    /* ENFORCE: Initialize scheduler state BEFORE registering tasks */
    unice64_scheduler_init();

    /* Register Idle Task in Slab 0 */
    register_task(idle_task, 0);

    /* Register System Maintenance Task in Slab 1 */
    register_task(system_task, 1);

    /* Register Shell Task in Slab 2 */
    if (pending_shell_entry) {
        register_task(pending_shell_entry, 2);
    }

    /* Context Guard: Verify that tasks were registered correctly */
    extern int get_task_count(void);
    PANIC_ON(get_task_count() < 3, "MULTITASKING_INIT: INSUFFICIENT SYSTEM TASKS");

    vga_print("[UNICE64] Multitasking initialized.\n");
}
