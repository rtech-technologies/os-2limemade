#include <stdint.h>
#include <stddef.h>

void vga_write_char(char c, uint8_t color_val);

void quartermaster_panic(const char* message, void* state) {
    (void)state;
    __asm__ volatile ("cli");
    const char* header = "\n[!] MECHANICAL FAILURE: ";
    for (int i = 0; header[i]; i++) vga_write_char(header[i], 0x4F);
    if (message) {
        for (int i = 0; message[i]; i++) vga_write_char(message[i], 0x4F);
    }
    const char* footer = "\n[!] SYSTEM HALTED. ALL CARGO LOST.\n";
    for (int i = 0; footer[i]; i++) vga_write_char(footer[i], 0x4F);
    for (;;) { __asm__ volatile ("hlt"); }
}
