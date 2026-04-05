.code64
.global idt_load
.global irq_timer_handler
.global exception_handler_stub
.extern apic_eoi
.extern unice64_context_switch

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

exception_handler_stub:
    cli
    # Simple Emerald (0x00FF88) Panic for Exceptions
    # In a real build, we would push the vector and call forensic_panic.
    1: hlt
    jmp 1b
