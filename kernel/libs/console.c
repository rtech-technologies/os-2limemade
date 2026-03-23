#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

void vga_write_char(char c, uint8_t color_val);
void serial_write_char(char c);

static uint8_t current_color = 0x07; /* White on Black */

void color(uint8_t fg, uint8_t bg) {
    current_color = (bg << 4) | (fg & 0x0F);
}

void print_cstr(const char* cstr) {
    for (int i = 0; cstr[i] != '\0'; i++) {
        vga_write_char(cstr[i], current_color);
        serial_write_char(cstr[i]);
    }
}

void print(managed_ptr_t str) {
    if (!str) return;
    print_cstr(str_to_cstr(str));
}

managed_ptr_t readline(void) {
    /* FREESTANDING PS/2 SIMULATION */
    /* In a real scenario, this would wait for IRQs from the PS/2 driver.
       For the Sovereign Core, we'll return a stub or implement a simple busy-wait for port 0x60. */
    static char buffer[128];
    int idx = 0;

    /* Simulate a prompt return for build verification */
    buffer[0] = 'h'; buffer[1] = 'e'; buffer[2] = 'l'; buffer[3] = 'l'; buffer[4] = 'o';
    buffer[5] = '\0';

    return str_create(buffer);
}
