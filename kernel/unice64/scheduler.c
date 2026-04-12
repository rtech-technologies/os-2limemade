#include "task.h"
#include <kernel/libs/storage/vdisk.h>
#include <stddef.h>

static task_t task_table[MAX_TASKS];
static uint8_t task_stacks[MAX_TASKS][TASK_STACK_SIZE] __attribute__((aligned(4096)));
static uint32_t task_bitmask = 0;
static int task_count = 0;
static int current_task_idx = 0;
static uint64_t burst_counter = 0;
static int forced_next_task = -1;
static volatile bool yield_signaled = false;

void vga_print(const char* fmt, ...);
void* pmm_alloc(uint64_t count);

uint64_t get_hhdm_offset(void);
uint64_t get_burst_count(void) { return burst_counter; }

void scheduler_force_task(int task_id) {
    forced_next_task = task_id;
}

bool scheduler_should_switch(void) {
    return yield_signaled;
}

void scheduler_clear_yield(void) {
    yield_signaled = false;
}

int get_ready_task_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if ((task_bitmask & (1 << i)) &&
           (task_table[i].state == TASK_READY || task_table[i].state == TASK_RUNNING || task_table[i].state == TASK_INPUT_WAIT)) {
            count++;
        }
    }
    return count;
}

void unice64_scheduler_init(void) {
    task_count = 0;
    current_task_idx = 0;
}

int register_task(void (*entry_point)(void), uint32_t slab_id) {
    task_t* current = get_current_task();
    for (int i = 0; i < MAX_TASKS; i++) {
        if (!(task_bitmask & (1 << i))) {
            task_bitmask |= (1 << i);
            task_table[i].id = i;
            task_table[i].state = TASK_READY;
            task_table[i].slab_id = slab_id;
            task_table[i].parent_id = current ? (int)current->id : -1;
            task_table[i].is_transient = false;
            task_table[i].in_use = true;

            uint64_t stack_virt = (uint64_t)&task_stacks[i];
            task_table[i].kernel_stack_top = stack_virt + TASK_STACK_SIZE;

            /* Initial Stack Frame for unice64_context_switch */
            uint64_t* stack = (uint64_t*)task_table[i].kernel_stack_top;

            /* ENFORCE: 16-byte Alignment for x86_64 */
            stack = (uint64_t*)(((uintptr_t)stack) & ~0x0F);

            *(--stack) = 0x10; /* SS */
            stack--;
            *stack = (uint64_t)stack + 8; /* RSP (Points to just after SS) */
            *(--stack) = 0x202; /* RFLAGS */
            *(--stack) = 0x08; /* CS */
            *(--stack) = (uint64_t)entry_point; /* RIP */

            /* 15 General Purpose Registers */
            for (int k = 0; k < 15; k++) *(--stack) = 0;

            task_table[i].context.rsp = (uint64_t)stack;

            if (task_count <= i) task_count = i + 1;
            vga_print("[UNICE64] Task %d registered in Slab %d\n", i, slab_id);
            return i;
        }
    }
    return -1;
}

int register_transient_task(void (*entry_point)(void), uint32_t slab_id, uint64_t arg) {
    task_t* current = get_current_task();
    for (int i = 0; i < MAX_TASKS; i++) {
        if (!(task_bitmask & (1 << i))) {
            task_bitmask |= (1 << i);
            task_table[i].id = i;
            task_table[i].state = TASK_READY;
            task_table[i].slab_id = slab_id;
            task_table[i].parent_id = current ? (int)current->id : -1;
            task_table[i].is_transient = true;
            task_table[i].in_use = true;

            void* slab_get_base(int id);
            void* slab_base = slab_get_base(slab_id);
            task_table[i].kernel_stack_top = (uint64_t)slab_base + (4 * 1024 * 1024);

            /* Initial Stack Frame */
            uint64_t* stack = (uint64_t*)task_table[i].kernel_stack_top;

            /* ENFORCE: 16-byte Alignment */
            stack = (uint64_t*)(((uintptr_t)stack) & ~0x0F);

            *(--stack) = 0x10; /* SS */
            stack--;
            *stack = (uint64_t)stack + 8; /* RSP */
            *(--stack) = 0x202; /* RFLAGS */
            *(--stack) = 0x08; /* CS */
            *(--stack) = (uint64_t)entry_point; /* RIP */

            /* 15 General Purpose Registers */
            for (int k = 0; k < 15; k++) *(--stack) = 0;

            /* RDI is at ctx_rdi (index 9 in the 15-register push block) */
            stack[9] = arg;

            task_table[i].context.rsp = (uint64_t)stack;

            if (task_count <= i) task_count = i + 1;
            vga_print("[UNICE64] Transient Task %d spawned in Slab %d\n", i, slab_id);
            return i;
        }
    }
    return -1;
}

task_t* get_current_task(void) {
    return &task_table[current_task_idx];
}

task_t* get_task_by_id(int id) {
    if (id < 0 || id >= MAX_TASKS) return NULL;
    if (!(task_bitmask & (1 << id))) return NULL;
    return &task_table[id];
}

int get_task_count(void) {
    return task_count;
}

void telemetry_update(int task_id, const char* status);

void sovereign_yield(void) {
    sys_yield();
}

void sys_yield(void) {
    bool tasking_is_scanning(void);
    if (tasking_is_scanning()) return;

    yield_signaled = true;
    /* Sovereign Wait: The task pauses here until the APIC Timer validates the yield */
    /* ENFORCE: Ensure interrupts are ON during wait so the timer can actually fire! */
    __asm__ volatile ("sti");
    while (yield_signaled) {
        __asm__ volatile ("pause");
    }
}

void vga_pulse_cursor(void);
void apic_timer_init(uint32_t count);

void unice64_schedule(void) {
    /* Update UI Pulse */
    vga_pulse_cursor();
    burst_counter++;

    /* Reset One-Shot Timer for next tick */
    apic_timer_init(500000);

    /* 1. Reaper Phase: Reclaim finished transient tasks */
    if (task_table[current_task_idx].state == TASK_ZOMBIE) {
        if (task_table[current_task_idx].is_transient) {
            void slab_release_transient(int id);
            slab_release_transient(task_table[current_task_idx].slab_id);
        }
        task_bitmask &= ~(1 << current_task_idx);
        task_table[current_task_idx].in_use = false;
    } else if (task_table[current_task_idx].state == TASK_RUNNING) {
        /* Task yielded voluntarily, mark as READY to be picked again */
        task_table[current_task_idx].state = TASK_READY;
    }

    /* 2. Selection Phase: Pick next task that is READY */
    bool found = false;

    if (forced_next_task != -1 && (task_bitmask & (1 << forced_next_task)) &&
       (task_table[forced_next_task].state == TASK_READY || task_table[forced_next_task].state == TASK_INPUT_WAIT)) {
        current_task_idx = forced_next_task;
        forced_next_task = -1;
        found = true;
    } else if (forced_next_task != -1 && task_table[forced_next_task].state == TASK_WAITING) {
        forced_next_task = -1;
    }

    if (!found) {
        /* Priority Selection: Check Tasks 1-15 first, Skip Idle (Task 0) if possible */
        for (int i = 1; i < MAX_TASKS; i++) {
            int idx = (current_task_idx + i) % MAX_TASKS;
            if (idx == 0) continue;
            if ((task_bitmask & (1 << idx)) &&
               (task_table[idx].state == TASK_READY || task_table[idx].state == TASK_INPUT_WAIT)) {
                current_task_idx = idx;
                found = true;
                break;
            }
        }
    }

    /* 3. Fallback Phase: If no other task is READY, go to Idle (Task 0) if it's READY */
    if (!found) {
        /* Last Second Audit: Check if any app became READY/INPUT_WAIT during the search */
        for (int i = 1; i < MAX_TASKS; i++) {
            if ((task_bitmask & (1 << i)) &&
               (task_table[i].state == TASK_READY || task_table[i].state == TASK_INPUT_WAIT)) {
                current_task_idx = i;
                found = true;
                break;
            }
        }

        if (!found && (task_table[0].state == TASK_READY || task_table[0].state == TASK_INPUT_WAIT)) {
            current_task_idx = 0;
            found = true;
        }
    }

    task_table[current_task_idx].state = TASK_RUNNING;

    /* Update Telemetry on every switch */
    telemetry_update(current_task_idx, "ACTIVE");
}
