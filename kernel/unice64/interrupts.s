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
    # 1. Forensic Anchorage: Save ALL GPRs immediately
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # SSE HARDENING: Save SSE state on stack to prevent corruption during C calls
    # We need 512 bytes + 16 byte alignment. Sub 528 ensures we have space even if aligned down.
    sub $528, %rsp
    mov %rsp, %rdi
    add $15, %rdi
    and $-16, %rdi
    fxsave (%rdi)

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

    # Restore SSE state before potential task switch
    add $528, %rsp

    # Restore ALL GPRs before full context switch takes over
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    # Jump into the full context switcher (it expects an iretq frame already on stack)
    jmp unice64_context_switch

.no_switch:
    # No yield signaled: Reset timer for next check (Wait again)
    # Fast Clock: 30ms interval
    mov $30000, %rdi
    call apic_timer_init

    # Restore SSE state
    mov %rsp, %rdi
    add $15, %rdi
    and $-16, %rdi
    fxrstor (%rdi)
    add $528, %rsp

    # Restore ALL GPRs
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax

    iretq

exception_handler_stub:
    cli
    # Simple Emerald (0x00FF88) Panic for Exceptions
    1: hlt
    jmp 1b
