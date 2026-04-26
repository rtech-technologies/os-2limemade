.code64
.global rsl_syscall_stub
.extern rsl_syscall_handler

rsl_syscall_stub:
    # Save caller registers
    push %rcx
    push %r11
    push %rbp
    mov %rsp, %rbp

    # 16-byte Alignment for ABI
    and $-16, %rsp

    # Call C handler
    # Arguments already in RDI, RSI, RDX, RCX (Wait, syscall uses different regs)
    # Our inline asm uses RDI, RSI, RDX. RAX is the ID.
    # C handler expects (ID=RDI, A1=RSI, A2=RDX, A3=RCX)
    mov %rdx, %rcx
    mov %rsi, %rdx
    mov %rdi, %rsi
    mov %rax, %rdi

    call rsl_syscall_handler

    # Restore registers and return
    mov %rbp, %rsp
    pop %rbp
    pop %r11
    pop %rcx
    iretq
