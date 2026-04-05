#include <stdint.h>
#include <stddef.h>

void vga_print(const char* fmt, ...);
uint64_t get_hhdm_offset(void);

static uintptr_t g_apic_base = 0;

#define APIC_ID       0x20
#define APIC_EOI      0x0B0
#define APIC_SPURIOUS 0x0F0
#define APIC_TIMER    0x320
#define APIC_TDCR     0x3E0
#define APIC_TICR     0x380

static inline void apic_write(uint32_t reg, uint32_t val) {
    if (!g_apic_base) return;
    volatile uint32_t* addr = (uint32_t*)(g_apic_base + reg);
    *addr = val;
}

static inline uint32_t apic_read(uint32_t reg) {
    if (!g_apic_base) return 0;
    volatile uint32_t* addr = (uint32_t*)(g_apic_base + reg);
    return *addr;
}

void apic_timer_init(uint32_t count) {
    /* Set Divide Configuration Register to 16 */
    apic_write(APIC_TDCR, 0x03);

    /* Set Local Vector Table Timer Register: Vector 32, One-Shot Mode */
    /* Handover Rule: Timer fires vector 32 for system_ticks */
    apic_write(APIC_TIMER, 32);

    /* Set Initial Count Register */
    apic_write(APIC_TICR, count);
}

void apic_init(void) {
    uint64_t hhdm = get_hhdm_offset();
    /* APIC Physical Base is 0xFEE00000. Access via HHDM virtual mapping. */
    g_apic_base = hhdm + 0xFEE00000;

    /* Spurious Interrupt Vector Register: Enable APIC, Vector 255 */
    apic_write(APIC_SPURIOUS, 0x1FF);

    void serial_write_str(const char* s);
    serial_write_str("[APIC] MMIO Aligned to High-Half virtual address.\n");
}

void apic_eoi(void) {
    apic_write(APIC_EOI, 0);
}
