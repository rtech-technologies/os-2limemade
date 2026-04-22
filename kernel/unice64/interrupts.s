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

.extern scheduler_should_switch
.extern scheduler_clear_yield
.extern apic_timer_init

irq_timer_handler:
    # Save partial state to allow C calls
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

    # Sovereign Logic: Check if a task has signaled a yield
    call scheduler_should_switch
    test %al, %al
    jz .no_switch

    # Yield detected: Clear the signal and perform full context switch
    call scheduler_clear_yield

    # Restore partial state before full context switch takes over
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rax

    # Jump into the full context switcher (it expects an iretq frame already on stack)
    jmp unice64_context_switch

.no_switch:
    # No yield signaled: Reset timer for next check (Wait again)
    # Fast Clock: 30ms interval
    mov $30000, %rdi
    call apic_timer_init

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

    iretq

exception_handler_stub:
    cli
    # Simple Emerald (0x00FF88) Panic for Exceptions
    # In a real build, we would push the vector and call forensic_panic.
    1: hlt
    jmp 1b
