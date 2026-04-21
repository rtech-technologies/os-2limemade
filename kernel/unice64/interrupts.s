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
    pushq %rax
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %rbp
    movq %rsp, %rbp
    movq 80(%rsp), %rdi
    movq 48(%rsp), %rsi
    movq 56(%rsp), %rdx
    movq 64(%rsp), %rcx
    movq 24(%rsp), %r8
    movq 32(%rsp), %r9
    call rsl_syscall_handler
    popq %rbp
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    addq $8, %rsp
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
