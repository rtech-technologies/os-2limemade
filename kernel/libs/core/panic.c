#include <stdint.h>
#include <stddef.h>

void vga_write_char(char c, uint8_t color_val);

void quartermaster_panic(const char* message, void* state) {
    (void)state; (void)message;
    /* [ Quartermaster Order: Atomic Terminal State ] */
    __asm__ volatile ("cli");
    const char* m = "\n[!] QUARTERMASTER: MECHANICAL TRUTH VIOLATED. CARGO ABORTED. SYSTEM HALTED.\n";
    for (int i = 0; m[i]; i++) vga_write_char(m[i], 0x4F);
    for (;;) { __asm__ volatile ("hlt"); }
}
