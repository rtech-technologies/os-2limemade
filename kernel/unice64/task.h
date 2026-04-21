#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_TASKS 16
#define TASK_STACK_SIZE 16384

typedef enum {
    TASK_RUNNING,
    TASK_READY,
    TASK_SLEEPING,
    TASK_WAITING,
    TASK_INPUT_WAIT,
    TASK_ZOMBIE
} task_state_t;

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
} cpu_context_t;

typedef struct {
    uint32_t id;
    task_state_t state;
    cpu_context_t context;
    uint32_t slab_id;
    int parent_id;
    uint64_t kernel_stack_top;
    bool is_transient;
    bool in_use;
    uint8_t padding[6];
    uint8_t fxsave_region[512] __attribute__((aligned(16)));
} task_t;

void unice64_schedule(void);
void unice64_scheduler_init(void);
int register_task(void (*entry_point)(void), uint32_t slab_id);
int register_transient_task(void (*entry_point)(void), uint32_t slab_id, uint64_t arg);
void sys_yield(void);
void scheduler_force_task(int task_id);
task_t* get_current_task(void);
task_t* get_task_by_id(int id);

#endif
