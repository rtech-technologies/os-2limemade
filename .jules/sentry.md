# Forensic Log - Sentry 🎯

## Slab Escape via Permissive Pointer Translation

**Date:** 2024-04-29
**Component:** `kernel/libs/core/rsl_syscalls.c`
**Severity:** Critical (Memory Trespassing)

### Root Cause Analysis
The `translate_user_ptr` function in the RSL syscall gateway allowed absolute high-half addresses (e.g., `0xFFFFFFFF80000000`) to be returned without any boundary checks if the address was already in the high-memory range. This allowed standalone "transient" RSL programs to "escape" their designated 4MB slab by simply passing a kernel address as a pointer argument.

Furthermore, several syscall handlers (ID 4-8) were passing raw `void*` arguments directly from the task's registers to kernel functions without calling `translate_user_ptr`. This bypasses the isolation logic entirely, even for relative offsets.

### The Fix
1.  **Strict Slab Isolation:** Modified `translate_user_ptr` to enforce that transient tasks can ONLY access memory within their assigned 4MB slab.
    - For absolute addresses (High-Half), we now verify they fall between `base_addr` and `base_addr + 4MB`.
    - Relative offsets are checked against the 4MB limit before being added to the slab base.
2.  **Universal Translation:** Updated the `rsl_syscall_handler` to ensure ALL pointer-based arguments are passed through the hardened `translate_user_ptr` before being used by the kernel.
3.  **Header Consolidation:** Removed inline `#include` statements and redundant local declarations (e.g., `slab_get_base`) to ensure a clean, audit-friendly gateway.

### Rule 4 Compliance
This fix closes a major loophole in the system's "Property Rights" model, ensuring that transient code cannot touch what it does not own (kernel memory or other slabs).

## Dual-Stage ISO & CARGO Universal Installer

**Date:** 2024-04-29
**Component:** `Makefile`, `programs/cargo.c`, `scripts/fat_tool.py`, `kernel/unice64/main.c`
**Severity:** High (System Deployment & Coexistence)

### Root Cause Analysis
The previous installation process was either non-existent or relied on raw disk wipes that could destroy other operating systems (like Windows). There was no mechanism to deploy the system as a "guest" or "co-tenant" on a GPT-partitioned drive.

### The implementation
1.  **Dual-Stage Build System:**
    - `system-iso`: Generates the standard `os2_system.iso` with all kernel binaries and programs.
    - `installer-iso`: Generates `os2_installer.iso`. This ISO contains the `os2_system.iso` inside a `/CARGO/` directory. It uses a specialized `limine.cfg` with `KERNEL_CMDLINE=quiet`.
2.  **CARGO Utility (`programs/cargo.c`):**
    - Built using the Nuklear GUI framework.
    - **Detection Logic:** Checks for `BOOT:/CARGO/os2_system.iso`. If present, it enters "INSTALLER" mode. If not, it enters "REPAIR" mode.
    - **Isolation:** Performs GPT-aware partitioning.
    - **Install Mode:** Wipes the target disk, creates a new GPT layout (16MB ESP + Sovereign Data Partition), and prepares for image streaming.
    - **Repair Mode:** Mounts existing Sovereign partitions to overwrite corrupted binaries.
3.  **GPT Partitioning (`scripts/fat_tool.py`):**
    - Upgraded from raw FAT32 to a full GPT layout.
    - Implements Protective MBR (Type 0xEE).
    - Generates GPT Header and Partition Entries (ESP: `C12A7328-F81F-11D2-BA4B-00A0C93EC93B`, Sovereign Data: `EBD0A0A2-B9E5-4433-87C0-68B6B72699C7`).
    - Places the Sovereign filesystem at a standard offset (2048 sectors) to allow coexistence with other EFI loaders.
4.  **Kernel Integration:**
    - `main.c` now parses the kernel command line for `quiet`.
    - If `quiet` is found, the kernel spawns `task_cargo` instead of the default shell. `task_cargo` directly executes `BOOT:/bin/cargo.bin`.
    - AHCI driver updated to perform GPT discovery. It no longer assumes a raw volume; it scans the GPT partition table and registers each partition as a unique logical volume (e.g., `SATA0_P0`, `SATA0_P1`).

## Technical Debt & Build Integrity

**Date:** 2024-04-29
**Component:** `services.c`, `vfs.c`, `vdisk.c`, `programs/libc/`
**Severity:** Medium (Maintainability)

### Root Cause Analysis
Scattered inline includes and static visibility of core functions hampered the modularity of the storage and service subsystems. Userland programs were missing standard string/integer conversion utilities.

### The Fix
1.  **Include Normalization:** Consolidated all `#include` statements to the top of `services.c` and `vfs.c`. Removed duplicate includes of `config.h` and `mouse.h`.
2.  **Visibility:** Changed `vdisk_ls_root` from `static` to global to allow the RSL syscall handler to access it for disk discovery.
3.  **Userland libc:** Implemented `itoa` and `reverse` in `programs/libc/libc.c` to support numeric formatting in the CARGO GUI (e.g., "Drive 0", "Drive 1").
4.  **Hardware Stability:** Enforced a strict 100-cycle limit on AHCI polling to prevent kernel hangs if a drive fails mid-initialization.
