# RTECH OSx2: OSx2 Limemade Architecture Guide (Hyper-Specific)

This guide provides a detailed walkthrough of the **OSx2 OSx2 Limemade Core** architecture. You can follow along with the source code in each directory to understand the system's execution flow.

## 1. Boot & Memory Layout (High-Half)
The kernel transitions from the bootloader to a 64-bit high-half environment.

- **Linker Script (`boot/linker.ld`):** Sets the kernel base address to `0xffffffff80000000`. It defines three primary program headers: `text` (RX), `rodata` (R), and `data` (RW). Sections are aligned to `0x1000`.
- **Limine Requests (`kernel/unice64/limine_reqs.c`):** Contains the metadata structures used by the Limine bootloader to communicate the memory map and the Direct Mapping (HHDM) offset. Use `get_memmap()` and `get_hhdm_offset()` to access these responses.
- **Modern Flags (`Makefile`):** Kernel compilation uses `-mcmodel=kernel` to ensure 64-bit code correctly references high-half addresses.

## 2. The Ritual: Core Orchestration
Execution begins in the **Ritual entry point**.

- **Entry Point (`kernel/unice64/main.c`):** The `_start()` function initializes the system by calling `dispatch_event(EVENT_INIT)`. It then enters `EVENT_MAIN` and calls `shell_main()`.
- **Service Registry (`kernel/libs/services.c`):** Orchestrates the modular hardware drivers.
    - `register_service(service_func_t init_func)`: Adds a service to the global `services` array (max 16).
    - `dispatch_event(kernel_event_t event)`: Iterates through all registered services and triggers their event handlers.
- **Service Handler (`kernel/libs/vga_serial.c`):** Implements `vga_serial_service()`. During `EVENT_INIT`, it initializes the Serial COM1 (0x3F8) and clears the VGA buffer (0xB8000). **Note:** Every VGA write is mirrored to the serial port for remote forensics.

## 3. Managed RAM: ARC & Bump Allocation
OSx2 Limemade memory is strictly managed using reference counting.

- **Bump Allocator (`kernel/libs/bump_alloc.c`):** A fast `bump_alloc(size_t size)` function that returns memory from a fixed 16MB `heap`. It aligns all allocations to 8 bytes.
    - **OSx2 Limemade Reset:** If the heap exceeds a 90% threshold, the allocator automatically triggers `bump_reset()`, which unloads all managed objects (effectively clearing userspace memory) while keeping the kernel code intact. This is logged to the serial console.
- **ARC Manager (`kernel/libs/arc_mem.c`):**
    - `arc_alloc(size_t size)`: Allocates `size` plus an 8-byte `arc_header_t`.
    - `arc_header_t`: Stores the `ref_count`.
    - `retain(void* ptr)` / `release(void* ptr)`: Increment and decrement the ref count. Rule #4 ensures memory stays 'OSx2 Limemade' until an explicit `EVENT_CLEANUP` or release to 0.

## 4. Storage Architecture: /CONNECT & VDISK
Storage is managed through a Virtual Disk abstraction.

- **VDISK Bridge (`kernel/libs/vdisk.c`):** The `/CONNECT` registry is an array of `vdisk_node_t` structures. Each node defines `sector_size`, `total_lba`, and function pointers for `read_lba` and `write_lba`.
- **Signature Check (`kernel/libs/signature_check.c`):** Implements `is_sovereign_disk(int disk_id)`. It reads LBA 0 of a disk and verifies the presence of the `0xDEADBEEF` signature.
- **PCI XHCI Scanning (`kernel/libs/usb_xhci.c`):** Scans the PCI bus and registers any found XHCI controllers to the VDISK layer.

## 5. RSL (RTECH Standard Library)
The RSL is the native interface for userspace (`programs/shell.c`).

- **Public Header (`include/rsl.h`):** Defines the Pythonic RSL API using `void*` for managed objects.
- **String Management (`kernel/libs/rsl_string.c`):** Implements `str_create()`, `str_match()`, and `str_is_empty()`. Managed pointers include an 8-byte ARC header.
- **Console API (`kernel/libs/console.c`):**
    - `color(fg, bg)`: Sets the global `current_color`.
    - `print(const char* s)`: Writes text to both VGA and Serial.
    - `input(const char* prompt)`: Displays the prompt and returns an ARC-managed `void*` string.
- **RSL Shell Commands (`kernel/libs/rsl_commands.c`):** Implements high-level filesystem operations bridged to FatFS.

## 6. Forensic Panic System
If a fatal error occurs, the system triggers an **Autopsy**.

- **Autopsy (`kernel/libs/panic.c`):** The `forensic_panic()` function:
    1. Sets the console to Emerald Green (10, 0).
    2. Prints a diagnostic message and dumps the `cpu_state_t` (RAX-R15).
    3. Mandatory Step: Scans and dumps USB controller registers (XHCI_USBCMD, XHCI_USBSTS) to Serial COM1.
    4. Halts the CPU.

## 7. Build System & Tools
- **Makefile:** Primary targets are `kernel`, `iso`, and `run`.
- **`scripts/menuconfig.py`:** Configures `.config` parameters.
- **`scripts/fat_tool.py`:** Generates sparse `ramdisk.img` files and injects the `0xDEADBEEF` signature at LBA 0.
- **The Xorriso Ritual:** The `make iso` target performs a three-stage boot deployment:
    1. **Limine Bootstrapping:** The Makefile automatically clones and builds the Limine v7.x-binary branch into the `limine/` folder before proceeding.
    2. **Xorriso:** Packages the kernel and configuration into an ISO and marks the boot code location.
    3. **Bios-Install:** Modifies the first few bytes of the `.iso` file to include the Limine MBR, ensuring SeaBIOS recognizes the disk as a bootable OS device rather than just storage.

## 8. Verification & Execution
To verify that the OSx2 Limemade OS is functioning correctly:

1. **Build the Kernel:** Run `make kernel`. This should produce a `kernel.elf` from source with no errors.
2. **Build the ISO:** Run `make iso`. This should generate a `ramdisk.img` with the `0xDEADBEEF` signature and package it into `osx2.iso`.
3. **Run in QEMU:** Run `make run`.
   - **Expected Output:** The system should boot via Limine, mirror "Serial initialized" and "--- [ LIMEMADE OS v0.1 ] ---" to the terminal, and display the `os2> ` prompt in Emerald Green on the VGA buffer. Interactivity is achieved through the Pythonic `input()` call.
4. **RSL Enforcement:** Open `programs/shell.c`. It must **ONLY** include `<rsl.h>`. Any inclusion of kernel headers (e.g., `services.h`) is a violation of the tiered architecture.

## 9. Modern Standard: Serial Forensics
All VGA output is mirrored to Serial COM1. This ensures that even if the hardware display fails, the kernel's state and RSL shell interactions are captured for analysis.
