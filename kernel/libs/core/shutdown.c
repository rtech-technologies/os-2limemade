#include <include/rsl.h>
#include <stdint.h>

void ahci_flush_cache(int p);
void serial_write_str(const char* s);
void vga_clear(void);
void vga_set_cursor(int x, int y);

void rsl_shutdown(void) {
    serial_write_str("[SHUTDOWN] Flushing AHCI caches...\n");
    /* OSx2: Flush all active ports */
    for (int i=0; i<32; i++) ahci_flush_cache(i);

    serial_write_str("[SHUTDOWN] System ready for power-off.\n");
    vga_clear();
    vga_set_cursor(0, 0);
    print("you may now power off your pc\n");

    /* Attempt ACPI shutdown via I/O port 0x604 (QEMU) */
    __asm__ volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));

    for (;;) __asm__ volatile ("hlt");
}
