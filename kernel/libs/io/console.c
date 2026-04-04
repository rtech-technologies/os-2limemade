#include <include/rsl.h>
#include <kernel/unice64/task.h>
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
        /* Serial mirroring is handled inside vga_write_char */
    }
    sys_yield(); /* Sovereign Active-Relay Rule: Yield after print */
}

static void print_num(uint32_t n, int base) {
    char buf[32];
    int i = 0;
    if (n == 0) {
        vga_write_char('0', current_color_val);
        return;
    }
    const char* digits = "0123456789ABCDEF";
    while (n > 0) {
        buf[i++] = digits[n % base];
        n /= base;
    }
    while (i > 0) {
        vga_write_char(buf[--i], current_color_val);
    }
}

void vga_print(const char* fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%' && fmt[i+1] != '\0') {
            i++;
            if (fmt[i] == 'd') {
                int n = __builtin_va_arg(args, int);
                print_num(n, 10);
            } else if (fmt[i] == 'x') {
                uint32_t n = __builtin_va_arg(args, uint32_t);
                print_num(n, 16);
            } else if (fmt[i] == 's') {
                char* s = __builtin_va_arg(args, char*);
                print(s);
            }
        } else {
            vga_write_char(fmt[i], current_color_val);
        }
    }
    __builtin_va_end(args);
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

static char shift_scancode_map[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*',	/* 9 */
  '(', ')', '_', '+', '\b',	/* Backspace */
  '\t',			/* Tab */
  'Q', 'W', 'E', 'R',	/* 19 */
  'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',	/* Enter key */
    0,			/* 29   - Control */
  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',	/* 39 */
 '"', '~',   0,		/* Left shift */
 '|', 'Z', 'X', 'C', 'V', 'B', 'N',			/* 49 */
  'M', '<', '>', '?',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
};

static bool shift_pressed = false;

char get_char(void) {
    while (1) {
        /* 1. PS/2 Keyboard Polling */
        uint8_t status = inb(0x64);
        if (status & 1) {
            uint8_t scancode = inb(0x60);

            /* Filter out mouse data (if bit 5 is set) */
            if (status & 0x20) continue;

            /* Check for shift pressed/released */
            if (scancode == 0x2A || scancode == 0x36) {
                shift_pressed = true;
                continue;
            }
            if (scancode == 0xAA || scancode == 0xB6) {
                shift_pressed = false;
                continue;
            }

            /* Special case: ENTER Release scancode (0x1C | 0x80 = 0x9C) */
            if (scancode == 0x9C) return -1; /* Special Enter Release code */

            /* All other release scancodes (scancode | 0x80) are ignored */
            if (scancode & 0x80) continue;

            /* Break Signal: Escape (scancode 0x01) */
            if (scancode == 0x01) return 27;

            if (scancode < 128) {
                char c = shift_pressed ? shift_scancode_map[scancode] : scancode_map[scancode];
                if (c) return c;
            }
        }

        /* 2. Serial COM1 Polling (Printable Only + Control) */
        if (serial_received()) {
            char c = serial_read_char();
            if (c == 27) return 27; /* ESC */
            if (c == '\n' || c == '\r' || c == '\b' || (c >= 32 && c <= 126)) return c;
        }

        /* Sovereign Active-Relay: Yield while waiting for input */
        task_t* current = get_current_task();
        if (current) current->state = TASK_WAITING;
        sys_yield();
        if (current) current->state = TASK_RUNNING;

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

        /* Wait for Enter key to be released before returning to prevent typing loop */
        if (c == (char)-1) continue;

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
            }
        } else {
            buffer[idx++] = c;
            vga_write_char(c, current_color_val);
        }
    }

    buffer[idx] = '\0';
    return str_create(buffer);
}
