# Forensic Log - Sentry 🎯

## Slab Escape via Permissive Pointer Translation

**Date:** 2024-04-28
**Component:** `kernel/libs/core/rsl_syscalls.c`
**Severity:** Critical (Memory Trespassing)

### Root Cause Analysis
The `translate_user_ptr` function in the RSL syscall gateway allowed absolute high-half addresses (e.g., `0xFFFFFFFF80000000`) to be returned without any boundary checks if the address was already in the high-memory range. This allowed standalone "transient" RSL programs to "escape" their designated 4MB slab by simply passing a kernel address as a pointer argument.

Furthermore, several syscall handlers (ID 4-8) were passing raw `void*` arguments directly from the task's registers to kernel functions without calling `translate_user_ptr`. This bypasses the isolation logic entirely, even for relative offsets.

### The Fix
1.  **Strict Slab Isolation:** Modified `translate_user_ptr` to enforce that transient tasks can ONLY access memory within their assigned 4MB slab. Absolute addresses are now checked against the slab's physical base address, and relative offsets are checked for size.
2.  **Universal Translation:** Updated the `rsl_syscall_handler` to ensure ALL pointer-based arguments are passed through the hardened `translate_user_ptr` before being used by the kernel.

### Rule 4 Compliance
This fix closes a major loophole in the system's "Property Rights" model, ensuring that transient code cannot touch what it does not own (kernel memory or other slabs).

## Freestanding Failures & Implicit Declarations

**Date:** 2024-04-28
**Component:** `kernel/libs/usb/cherryusb/core/usbh_core.c`, `kernel/libs/storage/`
**Severity:** Medium (Build Integrity)

### Root Cause Analysis
Several kernel components were assuming the presence of standard headers (`string.h`, `stdlib.h`) or global declarations (`serial_write_str`, `sys_yield`) without explicit inclusion or declaration. In a freestanding environment, this leads to build regressions and unpredictable behavior.

### The Fix
1.  **Standard Headers:** Created `include/string.h` and `include/stdlib.h` to provide the minimal subset of standard functions required by the kernel.
2.  **Explicit Declarations:** Added missing headers and explicit function declarations to `usbh_core.c` and `rsl_syscalls.c` to ensure build consistency and type safety.
3.  **Keyword Compliance:** Replaced non-standard `asm` with `__asm__` in `programs/libc/libc.c` to ensure compatibility with strict compiler flags in the standalone build environment.

## Infinite Polling Stalls (Hardware Hangups)

**Date:** 2024-04-28
**Component:** `ahci.c`, `usb_xhci.c`, `vga_serial.c`
**Severity:** High (System Stability)

### Root Cause Analysis
Several drivers utilized unbounded `while` loops to poll hardware status. If the hardware failed to respond (e.g., due to an init-stall or physical failure), the kernel would hang indefinitely or panic, preventing the system from reaching a functional state or a fallback shell.

### The Fix
1.  **Fail-Soft Polling:** Modified `ahci_wait_status` to return an error code instead of panicking on timeout.
2.  **Background Recovery:** Implemented a registration mask (`g_registered_ports_mask`) and enhanced `ahci_hardware_audit` to attempt re-initialization of missing disks during background system maintenance (multitasking phase).
3.  **Bounded I/O:** Added iteration limits to serial and XHCI initialization loops. If hardware does not respond within a reasonable window, the driver now aborts gracefully, allowing the rest of the system to remain responsive.
