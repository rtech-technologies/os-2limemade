#include <stdint.h>
#include <stddef.h>

void vga_print(const char* fmt, ...);

#define APIC_BASE 0xFEE00000
#define APIC_ID    (APIC_BASE + 0x20)
#define APIC_EOI   (APIC_BASE + 0x0B0)
#define APIC_SPURIOUS (APIC_BASE + 0x0F0)
#define APIC_TIMER (APIC_BASE + 0x320)
#define APIC_TDCR  (APIC_BASE + 0x3E0)
#define APIC_TICR  (APIC_BASE + 0x380)

static inline void apic_write(uint32_t reg, uint32_t val) {
    volatile uint32_t* addr = (uint32_t*)(uintptr_t)reg;
    *addr = val;
}

static inline uint32_t apic_read(uint32_t reg) {
    volatile uint32_t* addr = (uint32_t*)(uintptr_t)reg;
    return *addr;
}

void apic_timer_init(uint32_t count) {
    /* Set Divide Configuration Register to 16 */
    apic_write(APIC_TDCR, 0x03);

    /* Set Local Vector Table Timer Register: Vector 32, One-Shot Mode */
    apic_write(APIC_TIMER, 32);

    /* Set Initial Count Register */
    apic_write(APIC_TICR, count);
}

void apic_init(void) {
    /* Spurious Interrupt Vector Register: Enable APIC, Vector 255 */
    apic_write(APIC_SPURIOUS, 0x1FF);

    vga_print("[APIC] Local APIC Initialized.\n");
}

void apic_eoi(void) {
    apic_write(APIC_EOI, 0);
}
