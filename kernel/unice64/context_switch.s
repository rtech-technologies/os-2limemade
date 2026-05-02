.code64
.global unice64_context_switch
.extern get_current_task
.extern unice64_schedule
.extern quartermaster_panic

# Static Offsets for task_t and cpu_context_t
.set task_t_context_OFFSET, 8

# cpu_context_t layout (8 bytes each)
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
    # Quartermaster: Scheduler Readiness Shield
    # Check if we have a current task before attempting save
    call get_current_task
    test %rax, %rax
    jnz 2f
    iretq

2:
    # 1. Save state of the task being switched OUT
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

    # Quartermaster: Mechanical State Preservation
    # CALLEE-SAVED Register R12 will hold our stack reference across the C call
    mov %rsp, %r12

    # Get the current TCB
    call get_current_task

    # Forensic Check: Null TCB
    test %rax, %rax
    jz 1f

    # TCB is valid, save context
    mov %rax, %rdi
    add $task_t_context_OFFSET, %rdi # %rdi = &current->context

    # Store general purpose registers from saved stack pointer (R12) to TCB
    mov 0(%r12), %rbx;  mov %rbx, ctx_r15(%rdi)
    mov 8(%r12), %rbx;  mov %rbx, ctx_r14(%rdi)
    mov 16(%r12), %rbx; mov %rbx, ctx_r13(%rdi)
    mov 24(%r12), %rbx; mov %rbx, ctx_r12(%rdi)
    mov 32(%r12), %rbx; mov %rbx, ctx_r11(%rdi)
    mov 40(%r12), %rbx; mov %rbx, ctx_r10(%rdi)
    mov 48(%r12), %rbx; mov %rbx, ctx_r9(%rdi)
    mov 56(%r12), %rbx; mov %rbx, ctx_r8(%rdi)
    mov 64(%r12), %rbx; mov %rbx, ctx_rbp(%rdi)
    mov 72(%r12), %rbx; mov %rbx, ctx_rdi(%rdi)
    mov 80(%r12), %rbx; mov %rbx, ctx_rsi(%rdi)
    mov 88(%r12), %rbx; mov %rbx, ctx_rdx(%rdi)
    mov 96(%r12), %rbx; mov %rbx, ctx_rcx(%rdi)
    mov 104(%r12), %rbx; mov %rbx, ctx_rbx(%rdi)
    mov 112(%r12), %rbx; mov %rbx, ctx_rax(%rdi)

    # Save iretq frame (15 registers deep from R12)
    mov (15 * 8 + 0)(%r12), %rbx; mov %rbx, ctx_rip(%rdi)
    mov (15 * 8 + 8)(%r12), %rbx; mov %rbx, ctx_cs(%rdi)
    mov (15 * 8 + 16)(%r12), %rbx; mov %rbx, ctx_rflags(%rdi)
    mov (15 * 8 + 24)(%r12), %rbx; mov %rbx, ctx_rsp(%rdi)
    mov (15 * 8 + 32)(%r12), %rbx; mov %rbx, ctx_ss(%rdi)

    # 2. ABI Alignment & Handover
    mov %rsp, %rbp
    and $-16, %rsp
    call unice64_schedule
    mov %rbp, %rsp

    # 3. Load state of the task being switched IN
    call get_current_task
    test %rax, %rax
    jz 1f

    mov %rax, %rsi
    add $task_t_context_OFFSET, %rsi # %rsi = &next->context

    # Switch to target task stack
    mov ctx_rsp(%rsi), %rax
    mov %rax, %rsp

    # Restore iretq frame
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

# Forensic Failure Handlers
1:  # NULL TCB
    lea .msg_null_tcb(%rip), %rdi
    xor %rsi, %rsi
    call quartermaster_panic

.msg_null_tcb: .asciz "UNICE64: CONTEXT SWITCH NULL TCB"
.section .note.GNU-stack,"",@progbits
