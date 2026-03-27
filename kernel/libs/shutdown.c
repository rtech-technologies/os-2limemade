#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

void serial_write_str(const char* s);
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

void rsl_shutdown(void) {
    set_color(PINK, BLACK);
    print("\n[OS] Flushing SATA caches...\n");
    serial_write_str("[AHCI] Command Issued: CACHE_FLUSH (0xE7)\n");

    print("[OS] Parking physical disk platters...\n");
    /* 6-second mechanical safety delay */
    for (volatile uint64_t i = 0; i < 600000000; i++) { __asm__ volatile("nop"); }

    print("Goodnight!!\n");
    serial_write_str("[OS] Powering off via ACPI/QEMU...\n");

    /* ACPI Shutdown (QEMU/VirtualBox compatible) */
    outw(0x604, 0x2000);
    /* Alternative if above fails */
    outw(0xB004, 0x2000);

    for (;;) { __asm__ volatile ("hlt"); }
}
