# RTECH OSx2: Threat Model & Risk Notes

## 1. System-Wide Threat Model
- **Trust Boundary:** Userland (RSL Programs) vs. Kernel (Sovereign Services).
- **Attack Surface:** Syscall Interface, PCI Bus, USB/SATA Hardware.
- **Threat Actors:** Malicious RSL Scripts, Compromised USB Devices, Hardware Faults.

## 2. Per-Subsystem Risk Notes

### NVMe Driver
- **Risk:** Silent Data Corruption (SDC).
- **Remediation:** PRP/SGL validation, completion queue status checking, phase bit verification.
- **Hardware Quirk:** Some vendors require specific doorbell stride or CC.IOCQES/CC.IOSQES values.

### xHCI Driver
- **Risk:** Controller Hang during BIOS Handover.
- **Remediation:** Deterministic BOHC polling with millisecond timeouts.
- **Hardware Quirk:** Intel/AMD platforms have different SMI behaviors during OS ownership request.

### PCI Discovery
- **Risk:** Driver Misbinding or Double-Initialization.
- **Remediation:** Guarded probe functions and standardized ID matching.

### RTC64 GUI
- **Risk:** Memory Exhaustion or Screen Tearing.
- **Remediation:** Double buffering with Delta-Move flush. 64-byte alignment for cache coherence.

### Security (KASAN/Fuzzing)
- **Risk:** False Positives or System Performance Degradation.
- **Remediation:** Opt-in build flags for sanitizers. Randomized syscall fuzzing to identify edge cases.

## 3. License Enforcement
- **Risk:** Attribution loss or unlicensed commercial use.
- **Remediation:** Immutable build-time manifest and signed build hashes. Optional server-side registration hook for compliance tracking.
