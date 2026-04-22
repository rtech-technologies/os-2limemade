# OSx2 Sovereign Startup Manual

This manual describes the refined kernel initialization ritual of the OSx2 system, prioritizing spec-compliant hardware discovery and user-driven execution.

## 1. Foundation: The Kernel Ritual
The entry point in `kernel/unice64/main.c` initializes the core foundation:
- **Stack Migration:** Switches to a 32KB kernel stack.
- **Memory & GDT:** Initializes the PMM, Sovereign Slab Allocator, and GDT.
- **Interrupts:** Pre-registers the IDT.
- **SSE Support:** Enables CR0/CR4 bits for SIMD operations.

## 2. Heartbeat: The Timer Forge
Crucially, the system heart (APIC Timer) is forged before any hardware drivers are initialized. This ensures that bounded polling loops using `pit_wait_ms` function reliably from the first moment.

## 3. Storage discovery: The "Sovereign Order"
The AHCI driver (`ahci.c`) follows a strict spec-compliant handshake:
1. **Stop:** Ensure the HBA port engine is stopped (`ST=0`, `FRE=0`).
2. **Assign:** Set memory addresses for the Command List and FIS area while the engine is idle.
3. **Reset:** Issue a COMRESET and wait for link establishment.
4. **Ignite:** Enable FIS Receive (`FRE`) and start the engine (`ST`).
5. **Identify:** Wait for the hardware signature to appear and register the `VDISK` node.

## 4. Discovery & Selection Gate
The kernel scans all physical hardware volumes for a Sovereign (FAT32) signature.
- **Success:** If a Sovereign volume is found, it is mounted as `BOOT` and the system proceeds to the Shell.
- **Fail:** If no bootable volume exists, the kernel presents a recovery menu:
    1. **Safe Mode:** Boots into the Ramdisk environment only.
    2. **Format Disk:** Allows the user to select an available hardware disk, format it with the Sovereign FAT32 filesystem, and prepare it for installation.

## 5. User-Driven Execution
To maintain system integrity, the kernel does **not** automatically execute arbitrary binaries during startup.
- All external RSL binaries or scripts must be launched manually by the user via the `shell.c` interface.
- Automatic script execution is disabled to ensure the operator remains in full control of the "Mechanical Truth".
- Path-based invocation (`./path/to/bin`) is supported within the shell for user convenience.

## 6. Development Rule: The Foundry Forge
All binaries for this OS must be produced via the **Foundry Forge (Makefile)**.
- Manual compilation or outside binary injection is discouraged to maintain architectural purity.
- Use `make all` or `make kernel` to rebuild the system artifacts.
