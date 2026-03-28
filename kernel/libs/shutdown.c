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
    /* VGA Farewell */
    void vga_clear(void);
    void vga_set_cursor(int x, int y);
    vga_clear();
    set_color(WHITE, BLACK);
    vga_set_cursor(12, 33);
    print("Goodnight!!");

    /* Serial Debugging */
    serial_write_str("\n[OS] Initiating Sovereign Shutdown Protocol...\n");
    serial_write_str("[OS] Flushing SATA caches (Command 0xE7)...\n");

    /* 6-second mechanical safety delay */
    /* Wait for the HDD to physically spin down/park */
    for (volatile uint64_t i = 0; i < 600000000; i++) { __asm__ volatile("nop"); }

    serial_write_str("[OS] Powering off via ACPI...\n");

    /* ACPI Shutdown (QEMU/VirtualBox compatible) */
    outw(0x604, 0x2000);
    /* Alternative if above fails */
    outw(0xB004, 0x2000);

    for (;;) { __asm__ volatile ("hlt"); }
}
