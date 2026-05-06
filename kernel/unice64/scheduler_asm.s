.code64
.section .scheduler
.global unice64_context_switch
.extern get_current_task
.extern unice64_schedule
.extern quartermaster_panic_regs
.extern kernel_fallback_shell
.extern __text_start
.extern __text_end

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
    call get_current_task
    test %rax, %rax
    jnz 1f

    # NULL current task -> fallback
    call kernel_fallback_shell
    iretq

1:
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

    mov %rsp, %r12

    call get_current_task
    mov %rax, %rdi
    add $16, %rdi # Correct: id(4)+uid(4)+uaid(4)+state(4)=16

    # Store GPRs
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

    # Save iretq frame
    mov (15 * 8 + 0)(%r12), %rbx; mov %rbx, ctx_rip(%rdi)
    mov (15 * 8 + 8)(%r12), %rbx; mov %rbx, ctx_cs(%rdi)
    mov (15 * 8 + 16)(%r12), %rbx; mov %rbx, ctx_rflags(%rdi)
    mov (15 * 8 + 24)(%r12), %rbx; mov %rbx, ctx_rsp(%rdi)
    mov (15 * 8 + 32)(%r12), %rbx; mov %rbx, ctx_ss(%rdi)

    # 2. ABI Alignment & Handover
    mov %rsp, %rbp
    subq $32, %rsp
    and $-16, %rsp
    call unice64_schedule
    mov %rbp, %rsp

    # 3. Load state of the task being switched IN
    call get_current_task
    mov %rax, %rsi
    add $16, %rsi # Skip metadata (16 bytes)

    # RIP Safety Check
    mov ctx_rip(%rsi), %rdx
    test %rdx, %rdx
    jz .handle_invalid_rip

    # Boundary Check
    movabsq $__text_start, %rax
    cmp %rax, %rdx
    jb .handle_invalid_rip
    movabsq $__text_end, %rax
    cmp %rax, %rdx
    jae .handle_invalid_rip

    # RIP is valid, restore
    mov ctx_rsp(%rsi), %rax
    mov %rax, %rsp

    pushq ctx_ss(%rsi)
    pushq ctx_rsp(%rsi)
    pushq ctx_rflags(%rsi)
    pushq ctx_cs(%rsi)
    pushq ctx_rip(%rsi)

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
    mov ctx_rsi(%rsi), %rsi

    iretq

.handle_invalid_rip:
    # %rsi = &next->context, %rdx = invalid RIP
    mov %rdx, %rax           # Save invalid RIP
    mov 144(%rsi), %rdx      # RSP (Arg 3)
    mov %rax, %rsi           # RIP (Arg 2)
    lea .msg_bad_rip(%rip), %rdi # msg (Arg 1)

    # Quartermaster: ABI alignment for diagnostic call
    mov %rsp, %rbp
    subq $32, %rsp
    and $-16, %rsp
    call quartermaster_panic_regs
    call kernel_fallback_shell
    mov %rbp, %rsp
    iretq

.section .rodata
.msg_bad_rip: .asciz "UNICE64: INVALID SCHEDULER RIP"
.section .note.GNU-stack,"",@progbits
