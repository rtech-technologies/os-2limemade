# OSx2 Sovereign Startup Manual

This manual describes the refined kernel initialization ritual of the OSx2 system, prioritizing spec-compliant hardware discovery and user-driven execution.

## 1. Foundation: The Kernel Ritual
The entry point in `kernel/unice64/main.c` initializes the core foundation.

### Stack Migration & Early Debugging
The kernel migrates to a 32KB stack and initializes serial output immediately for telemetry.

```c
/* kernel/unice64/main.c */
void _start(void) {
    /* Switch to larger stack and ensure 16-byte alignment for SSE/ABI compliance */
    __asm__ volatile (
        "mov %0, %%rsp\n"
        "add $32768, %%rsp\n"
        "and $-16, %%rsp\n"
        "sub $8, %%rsp\n"
        : : "r" (kernel_stack) : "memory"
    );

    /* Initialize Serial immediately for debugging */
    serial_init();
    // ...
}
```

### Memory & Slab Allocation
The PMM and Sovereign Slab Allocator are initialized to provide identity-mapped physical and high-half virtual memory.

```c
/* kernel/unice64/main.c */
    vga_print("[BOOT] Initializing PMM...\n");
    pmm_init();
    vga_print("[BOOT] Initializing Slab Allocator...\n");
    slab_init();
```

### SSE Initialization
Modern driver state preservation relies on SSE instructions, which must be enabled early.

```c
/* kernel/unice64/main.c */
    /* SSE Initialization */
    vga_print("[BOOT] Enabling SSE/SIMD...\n");
    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); /* Clear EM bit */
    cr0 |= (1 << 1);  /* Set MP bit */
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    uint64_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);  /* Set OSFXSR bit */
    cr4 |= (1 << 10); /* Set OSXMMEXCPT bit */
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4));
```

## 2. Heartbeat: The Timer Forge
The system heart (APIC Timer) is initialized before hardware drivers to ensure reliable polling.

```c
/* kernel/unice64/main.c */
    /* Initialize Timer Hardware BEFORE device discovery */
    vga_print("[BOOT] Initializing APIC and Timer...\n");
    apic_init();
    apic_timer_init(50000);
```

## 3. Storage discovery: The "Sovereign Order"
The AHCI driver (`kernel/libs/storage/ahci.c`) follows a strict spec-compliant handshake:

```c
/* kernel/libs/storage/ahci.c */
    if (hba_base->cap2 & (1 << 0)) {
        vga_print("[AHCI] Requesting BIOS Handoff...\n");
        hba_base->bohc |= (1 << 1);
        int bohc_ms = 0;
        while ((hba_base->bohc & (1 << 0)) && bohc_ms < 1000) { pit_wait_ms(1); bohc_ms++; }
    }
    hba_base->ghc |= (1 << 31); hba_base->ghc |= (1 << 0);
    int ghc_ms = 0;
    while ((hba_base->ghc & (1 << 0)) && ghc_ms < 1000) { pit_wait_ms(1); ghc_ms++; }
    hba_base->ghc |= (1 << 31);
    vga_print("[AHCI] HBA Reset Complete.\n");
```

## 4. Discovery & Selection Gate
The kernel parses the command line to determine the boot mode and mount paths.

```c
/* kernel/unice64/main.c */
    if (quiet_mode) {
        tasking_create_kernel_thread(task_cargo, "cargo");
    } else {
        tasking_create_kernel_thread(task_shell, "shell");
    }
```

### Fallback Mechanism
If no physical Sovereign volume is found, the kernel attempts to promote the RAMDISK (INITRD) to `BOOT`.

```c
/* kernel/unice64/main.c */
    if (!mount_success) {
        vga_print("\n[CRITICAL] SYSTEM CANNOT FIND BOOT DISK.\n");
        /* ... Try to mount INITRD as fallback ... */
        if (ram_id != -1 && f_mount(&initrd_fs, ram_id) == FR_OK) {
            /* ... Register node ... */
            vga_print("[BOOT] INITRD promoted to BOOT node.\n");
            mount_success = true;
        }
    }
```

## 5. Persistence: The Mechanical Truth
Diagnostics are captured from the terminal buffer and synced to disk by a background thread.

```c
/* kernel/unice64/main.c */
void system_sync_task(void) {
    while (1) {
        void vga_sync_logs(void);
        vga_sync_logs();
        for (int i=0; i<5000; i++) sys_yield(); /* Sync every ~150s */
    }
}
```

## 6. Development Rule: The Foundry Forge
All binaries for this OS must be produced via the **Foundry Forge (Makefile)**. Use `make iso` to rebuild the system and installer images.
