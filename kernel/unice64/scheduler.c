#include "task.h"
#include <kernel/libs/storage/vdisk.h>
#include <stddef.h>

#define MAX_TASKS 16
#define TASK_STACK_SIZE 16384

static task_t task_table[MAX_TASKS];
static uint8_t task_stacks[MAX_TASKS][TASK_STACK_SIZE] __attribute__((aligned(4096)));
static int task_count = 0;
static int current_task_idx = 0;
static bool scheduler_active = false;

void vga_print(const char* fmt, ...);
void* pmm_alloc(uint64_t count);

uint64_t get_hhdm_offset(void);

void unice64_scheduler_init(void) {
    task_count = 0;
    current_task_idx = 0;
    scheduler_active = true;
}

void register_task(void (*entry_point)(void), uint32_t slab_id) {
    if (task_count < MAX_TASKS) {
        int idx = task_count;
        task_table[idx].id = idx;
        task_table[idx].state = TASK_READY;
        task_table[idx].slab_id = slab_id;

        /* Set Kernel Stack for Task (16KB aligned in Kernel Binary) */
        uint64_t stack_virt = (uint64_t)&task_stacks[idx];
        task_table[idx].kernel_stack_top = stack_virt + TASK_STACK_SIZE;

        /* Initialize Context */
        cpu_context_t* ctx = &task_table[idx].context;
        /* Zero out context for deterministic startup */
        uint8_t* p = (uint8_t*)ctx;
        for (size_t i = 0; i < sizeof(cpu_context_t); i++) p[i] = 0;

        ctx->rip = (uint64_t)entry_point;
        ctx->cs = 0x08; /* Kernel Code Segment */
        ctx->ss = 0x10; /* Kernel Data Segment */
        ctx->rflags = 0x202; /* Interrupts Enabled */

        /* 16-byte Alignment Trick for ABI compatibility */
        ctx->rsp = task_table[task_count].kernel_stack_top - 8;

        task_count++;
        vga_print("[UNICE64] Task registered in Slab %d\n", slab_id);
    }
}

task_t* get_current_task(void) {
    if (task_count == 0) return NULL;
    return &task_table[current_task_idx];
}

int get_task_count(void) {
    return task_count;
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
        *id = task_table[idx].id;
        *state = task_state_to_str(task_table[idx].state);
        *slab = task_table[idx].slab_id;
    }
}

void telemetry_update(int task_id, const char* status);

void task_exit(void) {
    task_t* current = get_current_task();
    if (current) {
        current->state = TASK_ZOMBIE;
        vga_print("[UNICE64] Task %d terminated.\n", current->id);
    }
    while (1) { sys_yield(); }
}

void sovereign_yield(void) {
    sys_yield();
}

void sys_yield(void) {
    __asm__ volatile ("int $0x81");
}

void vga_pulse_cursor(void);
void apic_timer_init(uint32_t count);

void unice64_schedule(void) {
    if (!scheduler_active) return;

    /* Update UI Pulse */
    vga_pulse_cursor();

    /* Reset One-Shot Timer for next tick */
    apic_timer_init(1000000);

    if (task_count < 2) return;

    /* Active-Relay Round Robin: Skip TASK_WAITING tasks */
    int next_idx = (current_task_idx + 1) % task_count;
    int loop_count = 0;
    while (task_table[next_idx].state != TASK_READY &&
           task_table[next_idx].state != TASK_RUNNING &&
           loop_count < task_count) {
        next_idx = (next_idx + 1) % task_count;
        loop_count++;
    }

    /* If no tasks are ready, use the first task (usually Idle) */
    if (loop_count >= task_count) next_idx = 0;

    if (task_table[current_task_idx].state == TASK_RUNNING) {
        task_table[current_task_idx].state = TASK_READY;
    }

    current_task_idx = next_idx;
    task_table[current_task_idx].state = TASK_RUNNING;

    /* Update Telemetry on every switch */
    telemetry_update(current_task_idx, "ACTIVE");
}
