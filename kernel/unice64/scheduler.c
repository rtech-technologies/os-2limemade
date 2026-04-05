#include "task.h"
#include <kernel/libs/storage/vdisk.h>
#include <stddef.h>

#define MAX_TASKS 16
static task_t task_table[MAX_TASKS];
static int task_count = 0;
static int current_task_idx = 0;

void vga_print(const char* fmt, ...);
void* pmm_alloc(uint64_t count);

uint64_t get_hhdm_offset(void);

void unice64_scheduler_init(void) {
    task_count = 0;
    current_task_idx = 0;
}

void register_task(void (*entry_point)(void), uint32_t slab_id) {
    if (task_count < MAX_TASKS) {
        task_table[task_count].id = task_count;
        task_table[task_count].state = TASK_READY;
        task_table[task_count].slab_id = slab_id;

        /* Allocate Kernel Stack for Task (16KB for Nested-VM Safety) */
        uint64_t stack_phys = (uint64_t)pmm_alloc(4); /* 4 pages = 16KB */
        uint64_t hhdm = get_hhdm_offset();
        uint64_t stack_virt = stack_phys + hhdm;

        task_table[task_count].kernel_stack_top = stack_virt + 16384;

        /* Initialize Context */
        cpu_context_t* ctx = &task_table[task_count].context;
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
    return &task_table[current_task_idx];
}

int get_task_count(void) {
    return task_count;
}

void telemetry_update(int task_id, const char* status);

void sovereign_yield(void) {
    sys_yield();
}

void sys_yield(void) {
    __asm__ volatile ("int $0x81");
}

void vga_pulse_cursor(void);
void apic_timer_init(uint32_t count);

void unice64_schedule(void) {
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
