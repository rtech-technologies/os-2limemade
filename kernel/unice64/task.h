#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TASK_RUNNING,
    TASK_READY,
    TASK_SLEEPING,
    TASK_WAITING,
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
    uint64_t kernel_stack_top;
    bool is_transient;
    bool in_use;
} task_t;

void unice64_schedule(void);
void unice64_scheduler_init(void);
void register_task(void (*entry_point)(void), uint32_t slab_id);
void register_transient_task(void (*entry_point)(void), uint32_t slab_id, uint64_t arg);
void sys_yield(void);
task_t* get_current_task(void);

#endif
