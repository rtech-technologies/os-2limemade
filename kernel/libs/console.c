#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

void vga_write_char(char c, uint8_t color_val);
void serial_write_char(char c);
char serial_read_char(void);
int serial_received(void);

static uint8_t current_color_val = 0x07; /* White on Black */

/* I/O Helpers */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void set_color(color_t fg, color_t bg) {
    current_color_val = ((uint8_t)bg << 4) | ((uint8_t)fg & 0x0F);
}

void print(const char* s) {
    if (!s) return;
    for (int i = 0; s[i] != '\0'; i++) {
        vga_write_char(s[i], current_color_val);
    }
}

/* Scancode to ASCII (Simplified US-QWERTY) */
static char scancode_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
  '9', '0', '-', '=', '\b',	/* Backspace */
  '\t',			/* Tab */
  'q', 'w', 'e', 'r',	/* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
    0,			/* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 '\'', '`',   0,		/* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
  'm', ',', '.', '/',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
};

char get_char(void) {
    while (1) {
        /* 1. PS/2 Keyboard Polling */
        if (inb(0x64) & 1) {
            uint8_t scancode = inb(0x60);

            /* Break Signal: Escape (scancode 0x01) */
            if (scancode == 0x01) return 27;

            if (scancode < 128 && scancode_map[scancode]) {
                return scancode_map[scancode];
            }
        }

        /* 2. Serial COM1 Polling */
        if (serial_received()) {
            char c = serial_read_char();
            if (c == 27) return 27; /* ESC */
            return c;
        }

        __asm__ volatile ("pause");
    }
}

void* input(const char* prompt) {
    if (prompt) {
        print(prompt);
    }

    char buffer[128];
    int idx = 0;

    while (idx < 127) {
        char c = get_char();

        /* Break logic: ESC */
        if (c == 27) {
            vga_write_char('^', current_color_val);
            vga_write_char('C', current_color_val);
            vga_write_char('\n', current_color_val);
            return NULL;
        }

        if (c == '\n' || c == '\r') {
            vga_write_char('\n', current_color_val);
            break;
        } else if (c == '\b') {
            if (idx > 0) {
                idx--;
                vga_write_char('\b', current_color_val);
                vga_write_char(' ', current_color_val);
                vga_write_char('\b', current_color_val);
            }
        } else {
            buffer[idx++] = c;
            vga_write_char(c, current_color_val);
        }
    }

    buffer[idx] = '\0';
    return str_create(buffer);
}
