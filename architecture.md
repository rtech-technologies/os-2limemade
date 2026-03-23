# RTECH OSx2: Sovereign Architecture Guide

This document outlines the tiered architecture of the **OSx2 Sovereign Core**, a 64-bit high-half kernel designed for USB support, managed memory, and RSL (RTECH Standard Library) interaction.

## 1. Memory Map & The High-Half
The kernel is linked to the high-half at `0xffffffff80000000`. Limine handles the initial boot and provides:
- **Direct Mapping (HHDM):** Allows the kernel to access physical memory at a fixed offset.
- **Memory Map:** Used to identify available RAM for the bump allocator.
- **VGA/Serial Mapping:** VGA (0xB8000) and Serial (0x3F8) are accessed through the high-half mapping for legacy fallback and forensic debugging.

## 2. The Ritual: Service Registry & Events
The kernel follows an event-driven lifecycle. `main.c` is a pure orchestrator that dispatches events to registered services.

### Lifecycle Events:
- **EVENT_INIT:** Initialize VGA/Serial, ARC Memory, PCI Bus Scanning (USB/XHCI), and VDISK registry.
- **EVENT_MAIN:** Mount Sovereign FAT32 volumes via `/CONNECT` and launch the **RSL Shell**.
- **EVENT_CLEANUP:** Prepare for shutdown by flushing FAT buffers and releasing resources.
- **EVENT_EXIT:** Final CPU halt.

## 3. Managed RAM: ARC & Bump Allocator
Memory management is built on **Automatic Reference Counting (ARC)**.
- **ARC Header:** Every allocation includes an 8-byte header storing the reference count.
- **Bump Allocator:** A high-speed, sequential allocator provides the backing store for the managed heap.
- **Sovereign Rule:** Memory remains allocated (Sovereign) until `EVENT_CLEANUP` or an explicit `release()` call.

## 4. Storage: /CONNECT & VDISK
Physical storage (USB, RAM Disk, SATA) is abstracted through the **VDISK Suit**.
- **/CONNECT Registry:** Tracks all physical storage nodes (LBA count and sector size).
- **Signature Handshake:** Disks must contain the `0xDEADBEEF` signature at LBA 0 to be recognized.
- **FatFS Integration:** FatFS is bridged to the VDISK layer, using the signature-checked nodes for file operations.

## 5. RSL (RTECH Standard Library)
The RSL is the only allowed interface for user-space programs.
- **Include Policy:** Programs must only include `<rsl.h>`.
- **API Highlights:** `readline()`, `color(fg, bg)`, and `rsl_f_open()` provide safe, managed access to kernel services.

## 6. Forensic Panic System
When a fatal error occurs (e.g., FAT corruption or USB timeout), the system enters **Autopsy**.
- **Emerald Display:** Failures are shown in `0xff88` Emerald Green.
- **Register Capture:** CPU state (RAX-R15) and USB controller registers are dumped to Serial COM1 (0x3f8) for post-mortem analysis.

## 7. Build System & Tooling
The build system is entirely source-based, with NO precompiled binaries allowed.
- **make menuconfig:** Terminal-based setup for Serial and Heap parameters.
- **make kernel:** Compiles the high-half ELF with FatFS linked.
- **make iso:** Packages the kernel, boot configuration, and a signed FAT32 ramdisk.
- **scripts/fat_tool.py:** Formats disk images and injects the `0xDEADBEEF` signature.
