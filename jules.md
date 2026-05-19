# Jules' Implementation Notes

## ⚓ Quartermaster's Mission: Boot Stability

### 1. Hardware Synchronization (Mechanical Truth)
- **AHCI BOHC**: Implemented the BIOS/OS Handoff. The kernel now waits up to 25ms for BIOS to release the controller, then falls back to waiting for the BIOS Busy (BB) bit.
- **AHCI GHC Reset**: Enforced a strict reset sequence. AE (AHCI Enable) is set, followed by HR (HBA Reset). The kernel polls for HR to clear, then re-enables AE and verifies it "sticks."
- **COMRESET**: Unified the COMRESET sequence for all ports. The DET bit is pulsed for 1ms, then the link state is polled for DET=3 (Physical link established).
- **XHCI Handover**: Added a robust handover for USB controllers, requesting ownership and disabling SMIs to prevent firmware interference.

### 2. Forensic Forensics (Deep Crimson)
- Implemented `quartermaster_panic` in `kernel/libs/core/panic.c`.
- Uses an 8x8 bitmap font for ASCII rendering without external dependencies.
- Displays register dumps and a basic stack trace to assist in cold-boot debugging.

### 3. Cargo Logistics
- **Installer State Machine**: `programs/cargo.c` now follows a 4-phase deployment (Identification, Partitioning, Formatting, Sync).
- **GPT Sovereignty**: `scripts/fat_tool.py` generates a GPT partition table with a "Sovereign" data partition starting at LBA 2048.
- **Payload Pathing**: Standardized the delivery of `cargo.bin` to `/bin/cargo.bin` in the ramdisk.

### 4. Stability Fixes
- **Context Switching**: Fixed a bug where the scheduler would clobber `%r12`, leading to "Ghost" crashes during high task loads.
- **Symbol Hygiene**: Purged UTF-8 symbols from the kernel binary to ensure compatibility with ASCII-only VGA text buffers, while maintaining Quartermaster's "Mechanical Truth" in comments.
