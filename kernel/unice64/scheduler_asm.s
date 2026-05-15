.code64
.section .scheduler
.global unice64_context_switch
.extern get_current_task
.extern unice64_schedule
.extern quartermaster_panic_regs
.extern kernel_fallback_shell
.extern __text_start
.extern __text_end

# task_t layout offsets (Based on C struct task_t in task.h)
.set task_t_id, 0
.set task_t_uid, 4
.set task_t_uaid, 8
.set task_t_state, 12
.set task_t_context, 16
.set task_t_slab_id, 176
.set task_t_kernel_stack_top, 184
.set task_t_last_rax, 192

# cpu_context_t layout (8 bytes each, starting at task_t_context)
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
    # 0. Scheduler Readiness Shield
    # Check if scheduler_active is true. Use absolute high-half address.
    movabsq $scheduler_active, %rax
    cmpb $0, (%rax)
    jne 1f
    iretq

1:
    # 1. IMMEDIATE Register Preservation
    # Save all GPRs before calling ANY C function to prevent register corruption.
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

    # Save current RSP to TCB
    mov %rsp, %r12
    call get_current_task
    # Sovereign: task_t.context starts at +16, .rsp is at +144 relative to context
    mov %r12, (task_t_context + ctx_rsp)(%rax)

    # 2. ABI Alignment & Handover
    mov %rsp, %rbp
    subq $32, %rsp
    and $-16, %rsp
    call unice64_schedule
    mov %rbp, %rsp

    # 3. Switch to IN task context
    call get_current_task
    # Sovereign: Restore RSP from next task TCB
    mov (task_t_context + ctx_rsp)(%rax), %rsp

    # RIP Safety Check (Look at restored stack: IRET frame is at top+120)
    # GPR frame is 15 * 8 = 120 bytes. RIP is the first element of IRET frame above GPRs.
    mov 120(%rsp), %rdx # RIP in IRET frame
    test %rdx, %rdx
    jz .handle_invalid_rip

    # Boundary Check (Canonical Higher-Half: Bit 63 must be set)
    movabsq $0x8000000000000000, %rax
    test %rax, %rdx
    jz .handle_invalid_rip

    # 4. Restore GPRs
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

    iretq

.handle_invalid_rip:
    # Save parameters for C call (Arg 2: RIP, Arg 3: RSP)
    # At entry: %rsi = &next->context, %rdx = invalid RIP
    mov %rdx, %rax           # Save invalid RIP
    mov ctx_rsp(%rsi), %rdx  # Load next->context.rsp into %rdx (Arg 3)
    mov %rax, %rsi           # Move invalid RIP into %rsi (Arg 2)
    lea .msg_bad_rip(%rip), %rdi # Arg 1: Message

    /* Quartermaster: Mechanical Truth Lock */
    cli

    # Quartermaster: ABI alignment for diagnostic call
    mov %rsp, %rbp
    subq $32, %rsp
    and $-16, %rsp

    # Mirror state to serial before panic
    # Using %r13 to save registers temporarily for serial_write_str
    # Note: We need a buffer for hex conversion, but we can call quartermaster_panic_regs
    # which is already updated in panic.c to do full mirror.

    call quartermaster_panic_regs
    call kernel_fallback_shell
    mov %rbp, %rsp
    iretq

.section .rodata
.msg_bad_rip: .asciz "UNICE64: INVALID SCHEDULER RIP"
.section .note.GNU-stack,"",@progbits
