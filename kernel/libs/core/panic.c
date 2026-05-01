#include <stdint.h>
#include <stddef.h>

void vga_write_char(char c, uint8_t color_val);

void panic(const char* message) {
    __asm__ volatile ("cli");
    /* Quartermaster: Atomic System Termination */
    const char* header = "\n[ QUARTERMASTER ] FATAL: ";
    for (int i = 0; header[i]; i++) vga_write_char(header[i], 0x4F);
    for (int i = 0; message[i]; i++) vga_write_char(message[i], 0x4F);
    vga_write_char('\n', 0x4F);
    for (;;) { __asm__ volatile ("hlt"); }
}

void forensic_panic(const char* message, void* state) {
    (void)state;
    panic(message);
}
