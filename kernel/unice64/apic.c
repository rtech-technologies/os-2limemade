#include <stdint.h>
#include <stddef.h>

void vga_print(const char* fmt, ...);

static uintptr_t g_apic_base = 0;

#define APIC_ID    (0x20)
#define APIC_EOI   (0x0B0)
#define APIC_SPURIOUS (0x0F0)
#define APIC_TIMER (0x320)
#define APIC_TDCR  (0x3E0)
#define APIC_TICR  (0x380)

static inline void apic_write(uint32_t reg, uint32_t val) {
    volatile uint32_t* addr = (uint32_t*)(g_apic_base + reg);
    *addr = val;
}

static inline uint32_t apic_read(uint32_t reg) {
    volatile uint32_t* addr = (uint32_t*)(g_apic_base + reg);
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

uint64_t get_hhdm_offset(void);

void serial_write_str(const char* s);
void serial_write_char(char c);

void apic_init(void) {
    /* Professional Startup Spinner: Mechanical Truth */
    serial_write_str("[INIT] Spinning hardware discovery gears...\n");
    for (int i=0; i<1000; i++) {
        static int gear = 0;
        const char* gear_chars = "|/-\\";
        if (i % 250 == 0) {
            serial_write_char('\r');
            serial_write_char(gear_chars[gear++ % 4]);
        }
    }
    serial_write_str("\n");

    uint64_t hhdm = get_hhdm_offset();
    g_apic_base = hhdm + 0xFEE00000;

    /* Spurious Interrupt Vector Register: Enable APIC, Vector 255 */
    apic_write(APIC_SPURIOUS, 0x1FF);

    vga_print("[APIC] Local APIC Initialized at Virtual 0x%x\n", g_apic_base);
}

void apic_eoi(void) {
    apic_write(APIC_EOI, 0);
}
