# RTECH OSx2: OSx2 Limemade Architecture Guide (Hyper-Specific)

This guide provides a detailed walkthrough of the **OSx2 OSx2 Limemade Core** architecture. You can follow along with the source code in each directory to understand the system's execution flow.

## 1. Boot & Memory Layout (High-Half)
The kernel transitions from the bootloader to a 64-bit high-half environment.

- **Linker Script (`boot/linker.ld`):** Sets the kernel base address to `0xffffffff80000000`. It defines three primary program headers: `text` (RX), `rodata` (R), and `data` (RW). Segments are explicitly page-aligned (`0x1000`) to prevent permission overlaps during loading.
- **Limine Requests (`kernel/unice64/limine_reqs.c`):** Contains the metadata structures used by the Limine bootloader to communicate the memory map and the Direct Mapping (HHDM) offset. Use `get_memmap()` and `get_hhdm_offset()` to access these responses.
- **Modern Flags (`Makefile`):** Kernel compilation uses `-mcmodel=kernel` to ensure 64-bit code correctly references high-half addresses.

## 2. The Ritual: Core Orchestration
Execution begins in the **Ritual entry point**.

- **Entry Point (`kernel/unice64/main.c`):** The `_start()` function initializes the system by calling `dispatch_event(EVENT_INIT)`. It then enters `EVENT_MAIN` and calls `shell_main()`.
- **Service Registry (`kernel/libs/services.c`):** Orchestrates the modular hardware drivers.
    - `register_service(service_func_t init_func)`: Adds a service to the global `services` array (max 16).
    - `dispatch_event(kernel_event_t event)`: Iterates through all registered services and triggers their event handlers.
- **Service Handler (`kernel/libs/vga_serial.c`):** Implements `vga_serial_service()`. During `EVENT_INIT`, it initializes the Serial COM1 (0x3F8) and the **GOP Framebuffer**.
    - **Font Rendering:** It uses an internal 8x8 font bitmap to draw characters to the screen.
    - **Forensic Mirroring:** Every VGA write is mirrored to the serial port for remote diagnostics.
    - **Backspace Support:** Implements logical backspace for interactive shell use.

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
- **Sovereign Discovery:** Disks are recognized as Sovereign based on GPT structure and hardware status in the registry.
- **PCI XHCI Scanning (`kernel/libs/usb_xhci.c`):** Scans the PCI bus for XHCI controllers and implements the **BIOS Handover Protocol**. It ensures the OS takes control of the controller registers from the BIOS.
- **SATA/AHCI Driver (`kernel/libs/ahci.c`):** Implements the **SATA Force Reset** protocol. It stops DMA engines, clears error registers, and performs a mechanical handshake (COMRESET) to establish a link (SSTS 0x03) before registering the device.
- **VFS Layer (`kernel/libs/vfs.c`):** Provides a unified interface for file operations using **Prefix-Based Routing** (e.g., `BOOT:/`, `INITRD:/`, `SATA0:/`). It dispatches requests to the appropriate filesystem handler with specific volume context (`void* priv`).

## 5. RSL (RTECH Standard Library)
The RSL is the native interface for userspace (`programs/shell.c`).

- **Public Header (`include/rsl.h`):** Defines the Pythonic RSL API using `void*` for managed objects.
- **String Management (`kernel/libs/rsl_string.c`):** Implements `str_create()`, `str_match()`, and `str_is_empty()`. Managed pointers include an 8-byte ARC header.
- **Console API (`kernel/libs/console.c`):**
    - `set_color(color_t fg, color_t bg)`: Sets the global console colors using a predefined `color_t` set.
    - `print(const char* s)`: Writes text to both VGA and Serial.
    - `input(const char* prompt)`: Displays the prompt and returns an ARC-managed `void*` string. This call implements **Unified Input Polling**, checking for input from the PS/2 Keyboard and Serial COM1.
- **RSL Shell Commands (`kernel/libs/rsl_commands.c`):** Implements high-level filesystem operations bridged to FatFS. The shell supports commands like `ls`, `cat`, `write`, `mkdir`, `rmdir`, `mount`, `format`, `stamp`, `run`, and `draw_rrif`.

## 6. Forensic Panic System
If a fatal error occurs, the system triggers an **Autopsy**.

- **Autopsy (`kernel/libs/panic.c`):** The `quartermaster_panic()` function:
    1. Sets the console to Emerald Green (10, 0).
    2. Prints a diagnostic message and dumps the `cpu_state_t` (RAX-R15).
    3. Mandatory Step: Scans and dumps USB controller registers (XHCI_USBCMD, XHCI_USBSTS) to Serial COM1.
    4. Halts the CPU.

## 7. Build System & Tools
- **Makefile:** Primary targets are `kernel`, `iso`, and `run`.
- **`scripts/menuconfig.py`:** Configures `.config` parameters.
- **`scripts/fat_tool.py`:** Generates FAT32 disk images with a primary GPT partition at LBA 2048.

## 8. Verification & Execution
To verify that the OSx2 Limemade OS is functioning correctly:

1. **Build the Kernel:** Run `make kernel`. This should compile all source files into `kernel.elf`. Binary artifacts are explicitly excluded and managed via `.gitignore`.
2. **Launch System:** Run `make run`.
   - **Expected Output:** The system should boot via Limine, scan AHCI ports for linked drives, discover the Sovereign partition at LBA 2048, mount it as `BOOT:/`, and launch the RSL Shell.
3. **Mechanical Truth:** Use the `write` command to create a file on `BOOT:/`. The system implements cluster allocation, and the file will persist across reboots.

## 9. Modern Standard: Serial Forensics
All VGA output is mirrored to Serial COM1. This ensures that even if the hardware display fails, the kernel's state and RSL shell interactions are captured for analysis.
