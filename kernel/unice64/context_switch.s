.code64
.global unice64_context_switch
.extern get_current_task
.extern unice64_schedule

# Offsets for task_t
.set task_t_context_OFFSET, 8

# Offsets for cpu_context_t
.set ctx_r15, 0
.set ctx_r14, 8
.set ctx_r13, 16
.set ctx_r12, 24
.set ctx_r11, 32
.set ctx_r10, 40
.set ctx_r9,  48
.set ctx_r8,  56
.set ctx_rbp, 64
.set ctx_rdi, 72
.set ctx_rsi, 80
.set ctx_rdx, 88
.set ctx_rcx, 96
.set ctx_rbx, 104
.set ctx_rax, 112
.set ctx_rip, 120
.set ctx_cs,  128
.set ctx_rflags, 136
.set ctx_rsp, 144
.set ctx_ss,  152

unice64_context_switch:
    # Save general purpose registers to stack
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    # Get current task
    call get_current_task
    mov %rax, %rdi # rdi = task_t*
    add $task_t_context_OFFSET, %rdi # rdi = cpu_context_t*

    # Copy registers from stack to TCB
    mov $15, %rcx
    mov %rsp, %rsi
    rep movsq

    # Copy iretq frame from stack to TCB
    # Frame at [rsp + 15*8]: RIP, CS, RFLAGS, RSP, SS
    mov 15*8(%rsp), %rax
    mov %rax, ctx_rip(%rdi)
    mov (15*8 + 8)(%rsp), %rax
    mov %rax, ctx_cs(%rdi)
    mov (15*8 + 16)(%rsp), %rax
    mov %rax, ctx_rflags(%rdi)
    mov (15*8 + 24)(%rsp), %rax
    mov %rax, ctx_rsp(%rdi)
    mov (15*8 + 32)(%rsp), %rax
    mov %rax, ctx_ss(%rdi)

    # Pick next task
    call unice64_schedule

    # Load next task
    call get_current_task
    mov %rax, %rsi # rsi = task_t*
    add $task_t_context_OFFSET, %rsi # rsi = cpu_context_t*

    # Switch to next task's stack for iretq
    mov ctx_rsp(%rsi), %rsp

    # Prepare iretq frame on new stack
    pushq ctx_ss(%rsi)
    pushq ctx_rsp(%rsi)
    pushq ctx_rflags(%rsi)
    pushq ctx_cs(%rsi)
    pushq ctx_rip(%rsi)

    # Restore general purpose registers
    mov ctx_r15(%rsi), %r15
    mov ctx_r14(%rsi), %r14
    mov ctx_r13(%rsi), %r13
    mov ctx_r12(%rsi), %r12
    mov ctx_r11(%rsi), %r11
    mov ctx_r10(%rsi), %r10
    mov ctx_r9(%rsi),  %r9
    mov ctx_r8(%rsi),  %r8
    mov ctx_rbp(%rsi), %rbp
    mov ctx_rdi(%rsi), %rdi
    mov ctx_rdx(%rsi), %rdx
    mov ctx_rcx(%rsi), %rcx
    mov ctx_rbx(%rsi), %rbx
    mov ctx_rax(%rsi), %rax
    mov ctx_rsi(%rsi), %rsi # Restore RSI last

    iretq
