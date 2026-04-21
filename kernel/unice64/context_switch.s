.code64
.global unice64_context_switch
.extern get_current_task
.extern unice64_schedule
.extern forensic_panic
.extern g_hhdm_offset

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
.set ctx_fxsave, 192

unice64_context_switch:
    # 1. Save Stage: Push all GPRs onto the current task's stack
    # This matches the layout of cpu_context_t if interpreted from RSP
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

    # 2. Anchorage: Save the current stack pointer into the TCB
    call get_current_task
    test %rax, %rax
    jz 1f

    # Validate TCB address
    mov %rax, %rdi
    mov $0xffffffff80000000, %rbx
    cmp %rbx, %rdi
    jb 2f

    # Store RSP in current->context.rsp (Offset 152 = 8 + 144)
    mov %rsp, 152(%rax)

    # Save SSE state
    fxsave ctx_fxsave(%rax)

    # 3. Handover: Pick the next task to run
    # Align stack for C call
    mov %rsp, %rbp
    and $-16, %rsp
    call unice64_schedule
    mov %rbp, %rsp

    # 4. Restoration: Load the next task's stack pointer
    call get_current_task
    test %rax, %rax
    jz 1f

    # Validate the new task's RSP before switching
    mov 152(%rax), %rsi

    # Forensic Check: RSP Validity (Kernel or HHDM)
    mov %rsi, %rax
    mov $0xffffffff80000000, %rbx
    cmp %rbx, %rax
    jb 4f
    mov $0xffffffff80800000, %rbx
    cmp %rbx, %rax
    jb 5f # Valid Kernel RSP
4:
    mov g_hhdm_offset(%rip), %rbx
    test %rbx, %rbx
    jz 3f
    cmp %rbx, %rax
    jb 3f # Truly Out of Bounds
5:
    # Restore SSE state
    # RAX still points to the new task's TCB (from step 4)
    fxrstor ctx_fxsave(%rax)

    # Switch to the new task's stack
    mov %rsi, %rsp

    # 5. Recovery: Pop all GPRs and Return to task execution
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax

    # Restore the CPU state and jump back to the task's instruction pointer
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
