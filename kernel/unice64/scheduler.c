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

/* UNICE64: Context Switch Assembly Bridge */
__attribute__((section(".scheduler")))
void unice64_context_switch_asm(void);

__asm__(
".section .scheduler\n"
".global unice64_context_switch\n"
"unice64_context_switch:\n"
"    call get_current_task\n"
"    test %rax, %rax\n"
"    jnz 1f\n"
"    iretq\n"
"1:\n"
"    push %rax\n"
"    push %rbx\n"
"    push %rcx\n"
"    push %rdx\n"
"    push %rsi\n"
"    push %rdi\n"
"    push %rbp\n"
"    push %r8\n"
"    push %r9\n"
"    push %r10\n"
"    push %r11\n"
"    push %r12\n"
"    push %r13\n"
"    push %r14\n"
"    push %r15\n"
"    mov %rsp, %r12\n"
"    call get_current_task\n"
"    mov %rax, %rdi\n"
"    add $16, %rdi\n" /* Fix: task_t_context_OFFSET = 16 (id=4, uid=4, uaid=4, state=4) */
"    mov 0(%r12), %rbx;  mov %rbx, 0(%rdi)\n"
"    mov 8(%r12), %rbx;  mov %rbx, 8(%rdi)\n"
"    mov 16(%r12), %rbx; mov %rbx, 16(%rdi)\n"
"    mov 24(%r12), %rbx; mov %rbx, 24(%rdi)\n"
"    mov 32(%r12), %rbx; mov %rbx, 32(%rdi)\n"
"    mov 40(%r12), %rbx; mov %rbx, 40(%rdi)\n"
"    mov 48(%r12), %rbx; mov %rbx, 48(%rdi)\n"
"    mov 56(%r12), %rbx; mov %rbx, 56(%rdi)\n"
"    mov 64(%r12), %rbx; mov %rbx, 64(%rdi)\n"
"    mov 72(%r12), %rbx; mov %rbx, 72(%rdi)\n"
"    mov 80(%r12), %rbx; mov %rbx, 80(%rdi)\n"
"    mov 88(%r12), %rbx; mov %rbx, 88(%rdi)\n"
"    mov 96(%r12), %rbx; mov %rbx, 96(%rdi)\n"
"    mov 104(%r12), %rbx; mov %rbx, 104(%rdi)\n"
"    mov 112(%r12), %rbx; mov %rbx, 112(%rdi)\n"
"    mov (15 * 8 + 0)(%r12), %rbx; mov %rbx, 120(%rdi)\n" /* RIP */
"    mov (15 * 8 + 8)(%r12), %rbx; mov %rbx, 128(%rdi)\n" /* CS */
"    mov (15 * 8 + 16)(%r12), %rbx; mov %rbx, 136(%rdi)\n" /* RFLAGS */
"    mov (15 * 8 + 24)(%r12), %rbx; mov %rbx, 144(%rdi)\n" /* RSP */
"    mov (15 * 8 + 32)(%r12), %rbx; mov %rbx, 152(%rdi)\n" /* SS */
"    mov %rsp, %rbp\n"
"    and $-16, %rsp\n"
"    call unice64_schedule\n"
"    mov %rbp, %rsp\n"
"    call get_current_task\n"
"    mov %rax, %rsi\n"
"    add $16, %rsi\n"
"    mov 144(%rsi), %rax\n"
"    mov %rax, %rsp\n"
"    pushq 152(%rsi)\n"
"    pushq 144(%rsi)\n"
"    pushq 136(%rsi)\n"
"    pushq 128(%rsi)\n"
"    pushq 120(%rsi)\n"
"    mov 0(%rsi), %r15\n"
"    mov 8(%rsi), %r14\n"
"    mov 16(%rsi), %r13\n"
"    mov 24(%rsi), %r12\n"
"    mov 32(%rsi), %r11\n"
"    mov 40(%rsi), %r10\n"
"    mov 48(%rsi), %r9\n"
"    mov 56(%rsi), %r8\n"
"    mov 64(%rsi), %rbp\n"
"    mov 72(%rsi), %rdi\n"
"    mov 88(%rsi), %rdx\n"
"    mov 96(%rsi), %rcx\n"
"    mov 104(%rsi), %rbx\n"
"    mov 112(%rsi), %rax\n"
"    mov 80(%rsi), %rsi\n"
"    iretq\n"
);

void unice64_scheduler_init(void) {
    task_count = 0;
    current_task_idx = 0;
    scheduler_active = true;
}

void register_task(void (*entry_point)(void), uint32_t slab_id) {
    if (task_count < MAX_TASKS) {
        int idx = task_count;
        task_table[idx].id = idx;
        task_table[idx].uid = 0;
        task_table[idx].state = TASK_READY;
        task_table[idx].slab_id = slab_id;

        uint64_t stack_virt = (uint64_t)&task_stacks[idx];
        task_table[idx].kernel_stack_top = stack_virt + TASK_STACK_SIZE;

        cpu_context_t* ctx = &task_table[idx].context;
        uint8_t* p = (uint8_t*)ctx;
        for (size_t i = 0; i < sizeof(cpu_context_t); i++) p[i] = 0;

        ctx->rip = (uint64_t)entry_point;
        ctx->cs = 0x08;
        ctx->ss = 0x10;
        ctx->rflags = 0x202;
        ctx->rsp = task_table[idx].kernel_stack_top - 8;

        task_count++;
        vga_print("[UNICE64] Task registered in Slab %d\n", slab_id);
    }
}

task_t* get_current_task(void) {
    if (task_count == 0) return NULL;
    return &task_table[current_task_idx];
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
        vga_print("[UNICE64] Found shell.bin, spawning...\n");
        void tasking_spawn_module(int module_index, uint32_t slab_id, uint32_t uaid);
        tasking_spawn_module(2, 1, 100);
    } else {
        kernel_fallback_shell();
    }
    while (1) { sys_yield(); __asm__ volatile ("pause"); }
}

void system_task(void) {
    vga_print("[UNICE64] System Maintenance Task Active.\n");
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
    if (get_task_count() < 3) {
        quartermaster_panic("RTECH: INSUFFICIENT RAM FOR MULTITASKING INITIALIZATION", NULL);
    }
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

void sovereign_yield(void) {
    sys_yield();
}
