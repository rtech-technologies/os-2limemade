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
    # Save all caller-saved registers
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

    # C Conv: rdi, rsi, rdx, rcx, r8, r9
    # We need: id (rax), a (rdi), b (rsi), c (rdx), d (r10), e (r8)

    # Arguments for rsl_syscall_handler(id, a, b, c, d, e)
    # rdi = rax (id)
    # rsi = rdi (a)
    # rdx = rsi (b)
    # rcx = rdx (c)
    # r8  = r10 (d)
    # r9  = r8  (e)

    movq 80(%rsp), %rdi  # Original rax (id)
    movq 48(%rsp), %rsi  # Original rdi (a)
    movq 56(%rsp), %rdx  # Original rsi (b)
    movq 64(%rsp), %rcx  # Original rdx (c)
    movq 24(%rsp), %r8   # Original r10 (d)
    movq 32(%rsp), %r9   # Original r8  (e)

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
    # Do not pop rax if we want to return a value, but rsl_syscall_handler is void for now.
    addq $8, %rsp # skip rax from stack, keep rax from call
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
