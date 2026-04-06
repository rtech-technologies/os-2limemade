# OSx2 Sovereign Startup Manual

This manual describes the synchronous kernel initialization ritual of the OSx2 Sovereign system, from the entry point to the RSL Shell, highlighting the "Mechanical Truth" of its implementation.

## 1. The Ritual: `_start`
The entry point of the OSx2 kernel is located in `kernel/unice64/main.c`.
- [Link to main.c](../kernel/unice64/main.c)

The kernel immediately switches from the Limine loader stack to a larger 32KB Sovereign Stack to prevent overflows during early initialization.

```c
/* The Ritual: Entry Point */
void _start(void) {
    /* Switch to larger stack before anything else */
    __asm__ volatile (
        "mov %0, %%rsp\n"
        "add $32760, %%rsp\n"  /* 16-byte Alignment Trick for x86_64 */
        : : "r" (kernel_stack) : "memory"
    );

    /* Sovereign Silicon Foundation */
    __asm__ volatile ("cli");
    gdt_init();
    pmm_init();
    slab_init();
    idt_init();

    /* Initialize Hardware and Core Memory */
    dispatch_event(EVENT_INIT);
    __asm__ volatile ("sti");
    // ...
}
```

## 2. Hardware Discovery: `EVENT_INIT`
The kernel dispatches `EVENT_INIT` to all services. The AHCI driver in `kernel/libs/storage/ahci.c` performs a synchronous hardware handshake.
- [Link to ahci.c](../kernel/libs/storage/ahci.c)

To avoid waiting for interrupts that haven't been "wired" by the Orchestrator, the driver uses `ahci_wait_status` to manually poll registers with a safety timeout.

```c
int ahci_wait_status(hba_port_t* port, uint32_t mask, uint32_t expected, uint32_t timeout_loops) {
    uint32_t count = 0;
    while (count < 1000000) {
        /* Task File Error Status (Bit 30 of PxIS) */
        if (port->is & (1 << 30)) {
            serial_print_hex32("[AHCI] TFES Detected! TFD: ", port->tfd);
            for(;;); /* HALT on silicon rejection */
        }

        /* Manual Poll: Check PxIS, PxTFD, or PxCI */
        if (port->is & mask) {
            port->is = 0xFFFFFFFF;
            return 0;
        }
        count++;
        __asm__ volatile ("pause");
    }
    panic("AHCI_POLL_TIMEOUT");
}
```

## 3. Storage & VFS Registration
After hardware discovery, the kernel performs volume registration in `main.c`. It scans detected hardware and mounts the filesystems using FatFS.

```c
for (int i = 0; i < hw_count; i++) {
    if (f_mount(&disk_fses[i], i) == FR_OK) {
        vfs_node_t node = {
            .private_data = &disk_fses[i],
            .ls = internal_fs_ls,
            /* ... */
        };
        vfs_register_node(node);
        vga_print("[FS] Drive %d Registered.\n", i);
    }
}
```

## 4. Tasking Engine Hard-Wiring
The multitasking engine is initialized in `kernel/unice64/tasking.c`.
- [Link to tasking.c](../kernel/unice64/tasking.c)

Tasks are registered in a strict linear order to ensure slab isolation. To guarantee boot stability, the engine avoids dynamic heap allocations during registration.

```c
void tasking_init(void) {
    /* Register Idle Task in Slab 0 */
    register_task(idle_task, 0);

    /* Register System Maintenance Task in Slab 1 */
    register_task(system_task, 1);

    /* Register Shell Task in Slab 2 */
    if (pending_shell_entry) {
        register_task(pending_shell_entry, 2);
    }

    unice64_scheduler_init();
}
```

## 5. Scheduler Ignition
The final step of `tasking_init` is calling `unice64_scheduler_init` in `kernel/unice64/scheduler.c`.
- [Link to scheduler.c](../kernel/unice64/scheduler.c)

The kernel dispatches its first `sys_yield()`, which triggers a software interrupt (`int $0x81`) to invoke the context switcher.

```c
/* kernel/unice64/main.c */
    /* Release yield-lock before handover */
    tasking_set_scanning(false);

    /* Start Scheduling */
    sys_yield();
```

## 6. Transient Slab Execution
Functions like `print()` utilize **Self-Destructing Tasks** and Micro-Slabs for isolated execution. This prevents system calls from polluting the caller's stack.
- [Link to console.c](../kernel/libs/io/console.c)

```c
void print(const char* s) {
    int slab_id = slab_grab_transient();

    print_request_t req = { .s = s, .done = false };
    /* Injection: Put request on the worker's future stack */
    print_request_t* remote_req = (print_request_t*)(stack_top - sizeof(print_request_t) - 16);
    *remote_req = req;

    register_transient_task(print_worker, slab_id);

    /* Suspend App until worker finishes */
    while (!remote_req->done) sys_yield();
}
```

## 7. The Command Interface: `shell_main`
The scheduler selects the Shell Task, which executes `shell_main` in `programs/shell.c`.
- [Link to shell.c](../programs/shell.c)

The Shell handles script execution and provides a pythonic command interface.

```c
void shell_main(void) {
    print("\n[ OSx2 Sovereign ] Build Success.\n");

    /* Automated Sovereignty: Execute startup script */
    rsl_execute_stream("BOOT:/BOOT.RSL");

    while (1) {
        void* cmd_line = input(str_to_cstr(prompt));
        rsl_execute_command(str_to_cstr(cmd_line));
    }
}
```
