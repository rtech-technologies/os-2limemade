.code64
.global idt_load
.global irq_timer_handler
.global rsl_syscall_entry
.global exception_handler_stub
.extern apic_eoi
.extern unice64_context_switch
.extern quartermaster_panic

idt_load:
    lidt (%rdi)
    ret

.extern timer_handler

irq_timer_handler:
    # Save partial state
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11

    # Send EOI to APIC
    call apic_eoi

    # Update system ticks
    call timer_handler

    # Restore partial state
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rax

    # Pure Cooperative: No context switch on timer
    iretq

.extern rsl_syscall_handler
rsl_syscall_entry:
    #  Quartermaster: Deterministic Syscall Register Handover
    # Save user state
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %rbx
    pushq %rbp

    # ABI Requirement: rdi, rsi, rdx, rcx, r8
    # User Input: RAX (id), RBX (arg1), RCX (arg2), RDX (arg3), RSI (arg4)
    # Mapping:
    # RDI <- RAX
    # RSI <- RBX
    # RDX <- RCX
    # RCX <- RDX
    # R8  <- RSI

    # Shuffle user registers to ABI order:
    movq %rsi, %r8   # arg4 -> R8
    movq %rdx, %r11  # Save arg3
    movq %rcx, %rdx  # arg2 -> RDX
    movq %r11, %rcx  # arg3 -> RCX
    movq %rbx, %rsi  # arg1 -> RSI
    movq %rax, %rdi  # id   -> RDI

    call rsl_syscall_handler

    # Restore user state
    popq %rbp
    popq %rbx
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx

    iretq

exception_handler_stub:
    cli
    movq $panic_msg, %rdi
    xorq %rsi, %rsi
    call quartermaster_panic
    1: hlt
    jmp 1b

.section .rodata
panic_msg: .asciz "CPU EXCEPTION"
