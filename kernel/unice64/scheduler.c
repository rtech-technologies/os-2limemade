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

        /* Allocate Kernel Stack for Task (8KB) */
        uint64_t stack_phys = (uint64_t)pmm_alloc(2); /* 2 pages = 8KB */
        uint64_t hhdm = get_hhdm_offset();
        uint64_t stack_virt = stack_phys + hhdm;

        task_table[task_count].kernel_stack_top = stack_virt + 8192;

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

void sovereign_yield(void) {
    __asm__ volatile ("int $32");
}

void unice64_schedule(void) {
    if (task_count < 2) return;

    /* Simple Round Robin Selection */
    int next_idx = (current_task_idx + 1) % task_count;
    while (task_table[next_idx].state != TASK_READY && task_table[next_idx].state != TASK_RUNNING) {
        next_idx = (next_idx + 1) % task_count;
    }

    if (task_table[current_task_idx].state == TASK_RUNNING) {
        task_table[current_task_idx].state = TASK_READY;
    }

    current_task_idx = next_idx;
    task_table[current_task_idx].state = TASK_RUNNING;
}
