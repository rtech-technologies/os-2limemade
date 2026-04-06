# OSx2 Sovereign Startup Manual

This manual describes the synchronous kernel initialization ritual of the OSx2 Sovereign system, from the entry point to the RSL Shell.

## 1. The Ritual: `_start`
The entry point of the OSx2 kernel is located in `_start` in `kernel/unice64/main.c`.
- [Link to main.c](../kernel/unice64/main.c)
- **Stack Switch:** The kernel immediately switches from the Limine loader stack to a larger 32KB Sovereign Stack.
- **Foundation:** The GDT, PMM, Slab Allocator, and IDT are initialized while interrupts are disabled (`cli`).

## 2. Hardware Discovery: `EVENT_INIT`
The kernel dispatches `EVENT_INIT` to all services.
- **AHCI Poll-only Bootstrap:** The AHCI driver in `kernel/libs/storage/ahci.c` performs a synchronous hardware handshake.
- [Link to ahci.c](../kernel/libs/storage/ahci.c)
- **Polling Logic:** To avoid waiting for "unwired" interrupts, the driver uses `ahci_wait_status` to manually poll the `PxIS`, `PxTFD`, and `PxCI` registers.
- **Physical Mapping:** All DMA structures (PRDT, CLB, FB) are mapped using raw physical addresses via `vmm_get_phys`.
- **Milestone Log:** Upon success, the kernel logs: `[INIT] AHCI Polling Success - Handoff to Orchestrator`.

## 3. Storage & VFS registration
After hardware discovery, the kernel performs volume registration in `main.c`:
- **BOOT Mount:** The Limine ramdisk is mounted as `BOOT:/`.
- **DISK Mount:** The first detected SATA HDD is mounted as `DISK0:/`.

## 4. Tasking Engine Hard-Wiring
The multitasking engine is initialized in `kernel/unice64/tasking.c`.
- [Link to tasking.c](../kernel/unice64/tasking.c)
- **Linear Registration:** Tasks are registered in a strict linear order to ensure slab isolation:
  - **Slab 0:** Idle Task
  - **Slab 1:** System Maintenance Task
  - **Slab 2:** RSL Shell Task
- **No-Bet Logic:** The tasking engine avoids dynamic heap allocations (no `str_create`) during registration to guarantee stability.

## 5. Scheduler Ignition
The final step of `tasking_init` is calling `unice64_scheduler_init` in `kernel/unice64/scheduler.c`.
- [Link to scheduler.c](../kernel/unice64/scheduler.c)
- **Cooperative Multitasking:** The system uses an Active-Relay cooperative round-robin model.
- **Handover:** The kernel dispatches its first `sys_yield()`, handing control to the scheduler.

## 6. The Command Interface: `shell_main`
The scheduler selects the Shell Task, which executes `shell_main` in `programs/shell.c`.
- [Link to shell.c](../programs/shell.c)
- **Choice-Gate:** The user is presented with a boot menu to install OSx2 or enter Safe Mode.
- **RSL Integration:** The shell provides a pythonic command interface powered by the RSL (Recursive Sovereign Logic) interpreter.
