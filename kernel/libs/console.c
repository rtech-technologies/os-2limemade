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
    }
}

void print(managed_ptr_t str) {
    if (!str) return;
    print_cstr(str_to_cstr(str));
}

managed_ptr_t input(managed_ptr_t prompt) {
    if (prompt) {
        print(prompt);
    }

    static char buffer[128];
    /* FREESTANDING PS/2 SIMULATION */
    /* Wait for user input (mocked for build verification) */
    buffer[0] = '\0';

    return str_create(buffer);
}
