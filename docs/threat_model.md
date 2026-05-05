# OSx2 Limemade Threat Model & Risk Assessment

## 1. Subsystem: Unice64 Kernel
*   **Asset**: System stability and memory integrity.
*   **Threat**: Syscall-driven memory corruption or privilege escalation.
*   **Mitigation**: Implemented ASAN/UBSAN instrumentation and a syscall fuzzing harness to identify edge-case crashes.
*   **Risk**: Medium. Fuzzer coverage is currently 1000 iterations; deeper formal verification of memory boundaries is needed.

## 2. Subsystem: NVMe Driver
*   **Asset**: Persistence and data integrity.
*   **Threat**: Incomplete command processing or silent data corruption during controller resets.
*   **Mitigation**: Implemented phase-bit based synchronization, mandatory PRP list alignment, and a robust reset state machine.
*   **Risk**: Low. Hardware variety testing (QD8 stress) ensures controller stability.

## 3. Subsystem: USB/xHCI & CherryUSB
*   **Asset**: Peripheral communication and mass storage access.
*   **Threat**: Malicious USB HID/MSC devices triggering buffer overflows in the stack.
*   **Mitigation**: BIOS Handover protocol ensures exclusive OS control; CherryUSB core utilized for hardened protocol parsing.
*   **Risk**: Medium. Integration of third-party stacks increases attack surface; IOCTL fuzzing implemented for driver interfaces.

## 4. Subsystem: RTC64 GUI & Nuklear
*   **Asset**: User interface and estate isolation.
*   **Threat**: UI-driven kernel denial of service or framebuffer disclosure.
*   **Mitigation**: Delta-Move Flush logic restricted to GOP framebuffer boundaries; vsync-aware synchronization to prevent tearing-based side channels.
*   **Risk**: Low. GUI runs in a restricted kernel-task context with limited memory access (backbuffer).

## 5. Subsystem: Build & License
*   **Asset**: Traceability and provenance.
*   **Threat**: Binary tampering or unauthorized modifications.
*   **Mitigation**: Immutable build-time manifest (SHA-256) embedded in kernel; prominent license UI with changelog.
*   **Risk**: Low. Signed hashes provide verification against build-time artifacts.

## 6. Hardware Test Checklist
- [ ] NVMe: Sequential/Random QD8 Stress (2+ Vendors)
- [ ] USB: Hotplug Stress (Intel/AMD Platforms)
- [ ] GOP: Flicker-free rendering (Multiple Implementations)
- [ ] Panic: Verify log reveal behavior on kernel exception
