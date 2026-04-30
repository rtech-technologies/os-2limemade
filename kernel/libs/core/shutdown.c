#include <kernel/libs/core/services.h>
#include <include/config.h>
#include <stdint.h>

void vga_print(const char* fmt, ...);
void vga_set_cursor(int x, int y);

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

void rsl_shutdown(void) {
    vga_set_cursor(0, 0);
    vga_print("\n[OS] Initiating Sovereign Shutdown Protocol...\n");
    vga_print("[OS] Powering off via ACPI...\n");
    outw(0x604, 0x2000);
}
