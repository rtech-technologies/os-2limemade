.code64
.global idt_load
.global irq_timer_handler
.global rsl_syscall_entry
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
    # Quartermaster: Deterministic Syscall Register Handover
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

.macro ISR_NOERRCODE num
.global isr\num
isr\num:
    pushq $0
    pushq $\num
    jmp exception_forensic_autopsy
.endm

.macro ISR_ERRCODE num
.global isr\num
isr\num:
    pushq $\num
    jmp exception_forensic_autopsy
.endm

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

exception_forensic_autopsy:
    # Quartermaster: Full State Capture (Matches struct cpu_state)
    # Stack at this point: [SS, RSP, RFLAGS, CS, RIP, ERR, NUM]
    # We need to push RAX, RBX, RCX, RDX, RSI, RDI, RBP, R8..R15

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

    # Passing (message, cpu_state*) to quartermaster_panic
    movq %rsp, %rsi          # RSI = &cpu_state
    lea panic_msg(%rip), %rdi # RDI = "CPU EXCEPTION"

    # Quartermaster: ABI Alignment for Panic
    # Align stack to 16-bytes for C call
    movq %rsp, %rbp
    andq $-16, %rsp
    call quartermaster_panic

    1: hlt
    jmp 1b

.section .rodata
panic_msg: .asciz "CPU EXCEPTION"
.section .note.GNU-stack,"",@progbits
