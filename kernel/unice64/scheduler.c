#include "task.h"
#include <include/rsl.h>
#include <kernel/libs/storage/vdisk.h>
#include <limine.h>
#include <stddef.h>

#define MAX_TASKS 16
#define TASK_STACK_SIZE 16384

static task_t task_table[MAX_TASKS];
static uint8_t task_stacks[MAX_TASKS][TASK_STACK_SIZE] __attribute__((aligned(4096)));
static int task_count = 0;
static int current_task_idx = 0;
static bool scheduler_active = false;

void vga_print(const char* fmt, ...);
void quartermaster_panic(const char* message, void* state);
void pit_wait_ms(uint32_t ms);
void vga_pulse_cursor(void);
void apic_timer_init(uint32_t count);
struct limine_module_response* get_modules(void);

task_t* get_current_task(void) {
    if (task_count == 0) return &task_table[0];
    if (current_task_idx < 0 || current_task_idx >= task_count) current_task_idx = 0;
    return &task_table[current_task_idx];
}

void unice64_scheduler_init(void) {
    task_count = 0;
    current_task_idx = 0;
    scheduler_active = false;
}

void idle_task(void);

void register_task(void (*entry_point)(void), uint32_t slab_id) {
    if (task_count < MAX_TASKS) {
        int idx = task_count;
        task_table[idx].id = idx;
        task_table[idx].uid = 0;
        task_table[idx].state = TASK_READY;
        task_table[idx].slab_id = slab_id;

        uint64_t stack_virt = (uint64_t)&task_stacks[idx];
        stack_virt = (stack_virt + 15) & ~0xFULL;
        task_table[idx].kernel_stack_top = stack_virt + TASK_STACK_SIZE;

        cpu_context_t* ctx = &task_table[idx].context;
        uint8_t* p_ctx = (uint8_t*)ctx;
        for (size_t i = 0; i < sizeof(cpu_context_t); i++) p_ctx[i] = 0;

        ctx->rip = (uint64_t)entry_point ? (uint64_t)entry_point : (uint64_t)idle_task;
        ctx->cs = 0x08;
        ctx->ss = 0x10;
        ctx->rflags = 0x202;
        ctx->rsp = task_table[idx].kernel_stack_top - 8;

        task_count++;
        vga_print("[UNICE64] Task registered in Slab %d\n", slab_id);
    }
}

int get_task_count(void) { return task_count; }

task_t* get_task_by_idx(int idx) {
    if (idx >= 0 && idx < task_count) return &task_table[idx];
    return NULL;
}

void idle_task(void) {
    while (1) {
        sys_yield();
        __asm__ volatile ("hlt");
    }
}

void kernel_fallback_shell(void);

void task_shell(void) {
    vga_print("[UNICE64] Shell Task Started.\n");
    struct limine_module_response* resp = get_modules();
    if (resp && resp->module_count >= 3) {
        void tasking_spawn_module(int module_index, uint32_t slab_id, uint32_t uaid);
        tasking_spawn_module(2, 1, 100);
    } else {
        kernel_fallback_shell();
    }
    while (1) { sys_yield(); __asm__ volatile ("pause"); }
}

void system_task(void) {
    static uint64_t last_audit = 0;
    extern uint64_t get_system_ticks(void);
    while (1) {
        uint64_t now = get_system_ticks();
        if (now - last_audit >= 5000) {
            last_audit = now;
            void ahci_hardware_audit(int p);
            ahci_hardware_audit(0);
        }
        sys_yield();
    }
}

void tasking_init(void) {
    unice64_scheduler_init();
    register_task(idle_task, 0);
    register_task(task_shell, 1);
    register_task(system_task, 2);
    scheduler_active = true;
    vga_print("[UNICE64] Multitasking initialized (3 tasks).\n");
}

void tasking_spawn_module(int module_index, uint32_t slab_id, uint32_t uaid) {
    struct limine_module_response* resp = get_modules();
    if (!resp || (uint64_t)module_index >= resp->module_count) return;
    struct limine_file* mod = resp->modules[module_index];
    if (!mod->address) return;
    uint8_t* ptr = (uint8_t*)mod->address;
    void (*entry)(void) = (void (*)(void))mod->address;
    if (ptr[0] == 'R' && ptr[1] == 'T' && ptr[2] == 'E' && ptr[3] == 'C' && ptr[4] == 'H') {
        rtech_header_t* header = (rtech_header_t*)mod->address;
        entry = (void (*)(void))((uintptr_t)mod->address + header->entry_offset);
    }
    register_task(entry, slab_id);
    task_t* t = get_task_by_idx(get_task_count() - 1);
    if (t) t->uaid = uaid;
}

void sys_yield(void) { __asm__ volatile ("int $0x81"); }

void telemetry_update(int task_id, const char* status);

void unice64_schedule(void) {
    if (!scheduler_active) return;
    vga_pulse_cursor();
    apic_timer_init(1000000);
    if (task_count < 2) return;
    int next_idx = (current_task_idx + 1) % task_count;
    int loop_count = 0;
    while (task_table[next_idx].state != TASK_READY &&
           task_table[next_idx].state != TASK_RUNNING &&
           loop_count < task_count) {
        next_idx = (next_idx + 1) % task_count;
        loop_count++;
    }
    if (loop_count >= task_count) next_idx = 0;
    if (task_table[current_task_idx].state == TASK_RUNNING) {
        task_table[current_task_idx].state = TASK_READY;
    }
    current_task_idx = next_idx;
    task_table[current_task_idx].state = TASK_RUNNING;
    telemetry_update(current_task_idx, "ACTIVE");
}

void quartermaster_panic_regs(const char* msg, uint64_t rip, uint64_t rsp) {
    vga_print("[PANIC] %s RIP=0x%x RSP=0x%x\n", msg, rip, rsp);
    quartermaster_panic(msg, NULL);
}

const char* task_state_to_str(task_state_t state) {
    switch(state) {
        case TASK_RUNNING: return "RUNNING";
        case TASK_READY:   return "READY";
        case TASK_SLEEPING:return "SLEEP";
        case TASK_WAITING: return "WAIT";
        case TASK_ZOMBIE:  return "ZOMBIE";
        default:           return "UNKNOWN";
    }
}

void get_task_info(int idx, uint32_t* id, const char** state, uint32_t* slab) {
    if (idx >= 0 && idx < task_count) {
        if (id) *id = task_table[idx].id;
        if (state) *state = task_state_to_str(task_table[idx].state);
        if (slab) *slab = task_table[idx].slab_id;
    }
}

void sovereign_yield(void) { sys_yield(); }
