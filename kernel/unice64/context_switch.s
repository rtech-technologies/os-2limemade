.code64
.global unice64_context_switch
.extern get_current_task
.extern unice64_schedule
.extern forensic_panic

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

    # Save SSE State (fxsave requires 16-byte alignment)
    sub $512, %rsp
    fxsave (%rsp)

    # Use %rax to store current %rsp for offset math
    mov %rsp, %rax

    # Get the current TCB
    call get_current_task

    # Forensic Check: Null TCB
    test %rax, %rax
    jz 1f

    # Forensic Check: TCB Boundary Validation
    # Kernel range: 0xffffffff80000000 - 0xffffffff80800000
    mov %rax, %rdi
    mov $0xffffffff80000000, %rbx
    cmp %rbx, %rdi
    jb 2f
    mov $0xffffffff80800000, %rbx
    cmp %rbx, %rdi
    jae 2f

    # TCB is valid, save context
    add $task_t_context_OFFSET, %rdi # %rdi = &current->context

    # Store general purpose registers from stack to TCB
    mov 0(%rax), %rbx; mov %rbx, ctx_r15(%rdi)
    mov 8(%rax), %rbx; mov %rbx, ctx_r14(%rdi)
    mov 16(%rax), %rbx; mov %rbx, ctx_r13(%rdi)
    mov 24(%rax), %rbx; mov %rbx, ctx_r12(%rdi)
    mov 32(%rax), %rbx; mov %rbx, ctx_r11(%rdi)
    mov 40(%rax), %rbx; mov %rbx, ctx_r10(%rdi)
    mov 48(%rax), %rbx; mov %rbx, ctx_r9(%rdi)
    mov 56(%rax), %rbx; mov %rbx, ctx_r8(%rdi)
    mov 64(%rax), %rbx; mov %rbx, ctx_rbp(%rdi)
    mov 72(%rax), %rbx; mov %rbx, ctx_rdi(%rdi)
    mov 80(%rax), %rbx; mov %rbx, ctx_rsi(%rdi)
    mov 88(%rax), %rbx; mov %rbx, ctx_rdx(%rdi)
    mov 96(%rax), %rbx; mov %rbx, ctx_rcx(%rdi)
    mov 104(%rax), %rbx; mov %rbx, ctx_rbx(%rdi)
    mov 112(%rax), %rbx; mov %rbx, ctx_rax(%rdi)

    # Save iretq frame (15 registers deep)
    mov (15 * 8 + 0)(%rax), %rbx; mov %rbx, ctx_rip(%rdi)
    mov (15 * 8 + 8)(%rax), %rbx; mov %rbx, ctx_cs(%rdi)
    mov (15 * 8 + 16)(%rax), %rbx; mov %rbx, ctx_rflags(%rdi)
    mov (15 * 8 + 24)(%rax), %rbx; mov %rbx, ctx_rsp(%rdi)
    mov (15 * 8 + 32)(%rax), %rbx; mov %rbx, ctx_ss(%rdi)

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

    # Forensic Check: RSP Boundary Validation
    mov ctx_rsp(%rsi), %rax
    mov $0xffffffff80000000, %rbx
    cmp %rbx, %rax
    jb 3f
    mov $0xffffffff80800000, %rbx
    cmp %rbx, %rax
    jae 3f

    # Switch to target task stack
    mov %rax, %rsp

    # Restore SSE State
    fxrstor (%rsp)
    add $512, %rsp

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
    call forensic_panic
2:  # TCB OUT OF BOUNDS
    lea .msg_tcb_bounds(%rip), %rdi
    xor %rsi, %rsi
    call forensic_panic
3:  # RSP OUT OF BOUNDS
    lea .msg_rsp_bounds(%rip), %rdi
    xor %rsi, %rsi
    call forensic_panic

.msg_null_tcb: .asciz "UNICE64: CONTEXT SWITCH NULL TCB"
.msg_tcb_bounds: .asciz "UNICE64: TCB ADDRESS OUT OF KERNEL BOUNDS"
.msg_rsp_bounds: .asciz "UNICE64: RSP ADDRESS OUT OF KERNEL BOUNDS"
