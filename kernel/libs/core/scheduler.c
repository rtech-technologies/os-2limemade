#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint64_t rip;
    uint64_t rsp;
    uint64_t rbp;
    uint64_t slab_id;
} task_context_t;

#define MAX_TASKS 4
static task_context_t tasks[MAX_TASKS];
static int current_task = 0;
static int task_count = 0;

void vga_print(const char* fmt, ...);

void scheduler_init(void) {
    task_count = 0;
    current_task = 0;
}

void register_task(uint64_t entry_point, uint64_t stack_ptr, uint64_t slab_id) {
    if (task_count < MAX_TASKS) {
        tasks[task_count].rip = entry_point;
        tasks[task_count].rsp = stack_ptr;
        tasks[task_count].rbp = stack_ptr;
        tasks[task_count].slab_id = slab_id;
        task_count++;
    }
}

/*
 * The Round Robin Switch:
 * In a pure Sovereign kernel, this would be triggered by Timer IRQ.
 * Here we provide the mechanical logic for the swap.
 */
void sovereign_yield(void) {
    if (task_count < 2) return;

    int next_task = (current_task + 1) % task_count;

    /* Logic: Swap contexts and Local Bump Pointers (Slabs) */
    current_task = next_task;
    // vga_print("[RR] Switched to Task %d\n", current_task);
}
