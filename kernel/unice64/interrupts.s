.code64
.global idt_load
.global irq_timer_handler
.global exception_handler_stub
.global rsl_syscall_stub
.extern apic_eoi
.extern unice64_context_switch
.extern timer_handler
.extern rsl_syscall_handler

idt_load:
    lidt (%rdi)
    ret

irq_timer_handler:
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    call apic_eoi
    call timer_handler
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rax
    iretq

rsl_syscall_stub:
    # Syscall: rax=id, rdi=a, rsi=b, rdx=c, r10=d, r8=e
    # C Conv: rdi, rsi, rdx, rcx, r8, r9
    pushq %rbp
    movq %rsp, %rbp

    # We must move them carefully to avoid clobbering
    # Save original rdi, rsi, rdx as they are used for C args 2, 3, 4
    pushq %rdi
    pushq %rsi
    pushq %rdx

    movq %r8, %r9   # e -> arg 6
    movq %r10, %r8  # d -> arg 5
    popq %rcx       # rdx -> arg 4 (from stack)
    popq %rdx       # rsi -> arg 3 (from stack)
    popq %rsi       # rdi -> arg 2 (from stack)
    movq %rax, %rdi # rax -> arg 1

    call rsl_syscall_handler

    popq %rbp
    iretq

exception_handler_stub:
    cli
    1: hlt
    jmp 1b

.global xhci_irq_stub
.extern xhci_irq_handler

xhci_irq_stub:
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    call apic_eoi
    call xhci_irq_handler
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rax
    iretq
