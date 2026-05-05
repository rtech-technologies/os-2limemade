# OS*2 Limemade Threat Model & Risk Assessment

## 1. Subsystem: NVMe Driver
- **Threat**: DMA Overwrite (PRP manipulation)
- **Risk**: High
- **Mitigation**: Kernel-side validation of all PRP addresses against Sovereign slab boundaries.
- **Threat**: Controller Hang (Infinite polling)
- **Risk**: Moderate
- **Mitigation**: Implemented millisecond-bounded timeouts for all status register polling.

## 2. Subsystem: USB/xHCI
- **Threat**: BIOS/OS Contention
- **Risk**: Moderate
- **Mitigation**: Standardized xHCI BIOS Handover protocol with semaphore synchronization.

## 3. Subsystem: GUI (RTC64/Nuklear)
- **Threat**: Framebuffer Overflow
- **Risk**: Moderate
- **Mitigation**: Aligned virtual double-buffering and strict bounds checking in `draw_pixel`.

## 4. Overall Architecture: Active-Relay
- **Threat**: Task starvation
- **Risk**: Low
- **Mitigation**: Round-robin scheduler with voluntary yields enforced in RSL API.
