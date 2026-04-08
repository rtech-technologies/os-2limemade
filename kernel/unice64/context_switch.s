.code64
.global unice64_context_switch
.extern get_current_task
.extern unice64_schedule
.extern forensic_panic

# Static Offsets for task_t and cpu_context_t
# task_t: id(4), state(4), context(168), slab_id(4), stack_top(8)
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
    # Save general purpose registers to current stack
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

    # Get current task TCB
    call get_current_task

    # Forensic Check: Null TCB
    test %rax, %rax
    jz 1f

    # Forensic Check: Boundary Validation
    # Kernel Base: 0xffffffff80000000. 8MB Limit: 0xffffffff80800000
    mov %rax, %rdi
    mov $0xffffffff80000000, %rbx
    cmp %rbx, %rdi
    jb 2f
    mov $0xffffffff80800000, %rbx
    cmp %rbx, %rdi
    jae 2f

    # TCB is valid, save state
    add $task_t_context_OFFSET, %rdi # rdi = &current->context
    mov %rsp, %rax # Use stack as source

    # Store all registers using STATIC OFFSETS to prevent drift
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

    # Handover Preparation: Alignment
    mov %rsp, %rbp
    and $-16, %rsp

    # Handover: Pick next task
    call unice64_schedule

    # Restore stack for restoration
    mov %rbp, %rsp

    # Load next task
    call get_current_task
    test %rax, %rax
    jz 1f

    mov %rax, %rsi
    add $task_t_context_OFFSET, %rsi # rsi = &next->context

    # Forensic Check: RSP Validity
    # Supports Kernel Base (0xffffffff80000000) and Dynamic HHDM Base
    mov ctx_rsp(%rsi), %rax

    # 1. Check if in Kernel Stack Range
    mov $0xffffffff80000000, %rbx
    cmp %rbx, %rax
    jb 4f
    mov $0xffffffff80800000, %rbx
    cmp %rbx, %rax
    jb 5f # Valid Kernel RSP

4:
    # 2. Check if in HHDM Range (Slab Stacks)
    # Using g_hhdm_offset variable for validation
    mov g_hhdm_offset(%rip), %rbx
    test %rbx, %rbx
    jz 3f # Offset not set, out of bounds

    cmp %rbx, %rax
    jb 3f # Truly Out of Bounds

5: # RSP is Valid

    # Switch to next task's kernel stack
    mov %rax, %rsp

    # Restore iretq frame
    pushq ctx_ss(%rsi)
    pushq ctx_rsp(%rsi)
    pushq ctx_rflags(%rsi)
    pushq ctx_cs(%rsi)
    pushq ctx_rip(%rsi)

    # Restore registers
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

# Forensic Panic Points
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
