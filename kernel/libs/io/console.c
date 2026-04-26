#include <include/rsl.h>
#include <kernel/unice64/task.h>
#include <stdint.h>
#include <stddef.h>

void vga_write_char(char c, uint8_t color_val);
void serial_write_char(char c);
void serial_write_str(const char* s);
char serial_read_char(void);
int serial_received(void);

void serial_print_num(uint32_t n, int base);

static uint8_t current_color_val = 0x07; /* White on Black */
bool g_vga_silent = false;
uint64_t g_kbd_initial_delay = 500;
uint64_t g_kbd_repeat_rate = 50;

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

    /* Mirroring Gate: vga_write_char handles both VGA and Serial output */
    for (int i = 0; s[i] != '\0'; i++) {
        vga_write_char(s[i], current_color_val);
    }
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
                if (s) {
                    for (int k = 0; s[k] != '\0'; k++) vga_write_char(s[k], current_color_val);
                }
            }
        } else {
            vga_write_char(fmt[i], current_color_val);
        }
    }
    __builtin_va_end(args);
}

void serial_print_num(uint32_t n, int base) {
    char buf[32];
    int i = 0;
    if (n == 0) {
        serial_write_char('0');
        return;
    }
    const char* digits = "0123456789ABCDEF";
    while (n > 0) {
        buf[i++] = digits[n % base];
        n /= base;
    }
    while (i > 0) {
        serial_write_char(buf[--i]);
    }
}

void serial_print(const char* fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%' && fmt[i+1] != '\0') {
            i++;
            if (fmt[i] == 'd') {
                int n = __builtin_va_arg(args, int);
                serial_print_num(n, 10);
            } else if (fmt[i] == 'x') {
                uint32_t n = __builtin_va_arg(args, uint32_t);
                serial_print_num(n, 16);
            } else if (fmt[i] == 's') {
                char* s = __builtin_va_arg(args, char*);
                serial_write_str(s);
            }
        } else {
            serial_write_char(fmt[i]);
        }
    }
    __builtin_va_end(args);
}

/* Scancode to ASCII (Simplified US-QWERTY) */
static char __attribute__((used)) scancode_map[128] = {
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

static char __attribute__((used)) shift_scancode_map[128] = {
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

static bool __attribute__((used)) shift_pressed = false;
extern bool control_pressed;

#include <include/mouse.h>
static int drag_start_x = -1;
static int drag_start_y = -1;

bool control_pressed = false;

#define KBD_BUF_SIZE 64
static char console_input_buffer[KBD_BUF_SIZE];
static int input_head = 0;
static int input_tail = 0;

void console_push_char(char c) {
    if (!c || c == (char)-1) return;
    int next = (input_tail + 1) % KBD_BUF_SIZE;
    if (next != input_head) {
        console_input_buffer[input_tail] = c;
        input_tail = next;
    }
}

char console_pop_char(void) {
    if (input_head == input_tail) return 0;
    char c = console_input_buffer[input_head];
    input_head = (input_head + 1) % KBD_BUF_SIZE;
    return c;
}

void hw_poll(void) {
    #include <include/config.h>

    /* 1. PS/2 Keyboard Polling */
#if defined(CONFIG_INTERFACE_ALL) || defined(CONFIG_INTERFACE_PS2)
    uint8_t status = inb(0x64);
    if (status & 1) {
        uint8_t scancode = inb(0x60);
        if (!(status & 0x20)) { /* Not Mouse Data */
            if (scancode == 0x2A || scancode == 0x36) shift_pressed = true;
            else if (scancode == 0xAA || scancode == 0xB6) shift_pressed = false;
            else if (scancode == 0x1D) control_pressed = true;
            else if (scancode == 0x9D) control_pressed = false;
            else if (!(scancode & 0x80)) {
                if (control_pressed && scancode == 0x2E) { /* Ctrl+C */
                    void console_copy_selection(void);
                    console_copy_selection();
                } else if (scancode == 0x01) console_push_char(27);
                else if (scancode < 128) {
                    char c = shift_pressed ? shift_scancode_map[scancode] : scancode_map[scancode];
                    if (c) console_push_char(c);
                }
            }
        }
    }
#endif

    /* 2. Serial COM1 Polling */
    if (serial_received()) {
        char c = serial_read_char();
        if (c == '\n' || c == '\r' || c == '\b' || c == 27 || (c >= 32 && c <= 126)) {
            console_push_char(c);
        }
    }

    /* 4. PS/2 Mouse Polling */
#if defined(CONFIG_INTERFACE_ALL) || defined(CONFIG_INTERFACE_PS2)
    void mouse_poll(void);
    mouse_poll();
#endif

    /* Selection Logic */
    mouse_state_t* ms = get_mouse_state();
    if (ms && ms->active) {
        if (ms->left_button) {
            if (drag_start_x == -1) {
                drag_start_x = ms->x;
                drag_start_y = ms->y;
            }
            void vga_set_selection(int x1, int y1, int x2, int y2);
            vga_set_selection(drag_start_x / 16, drag_start_y / 16, ms->x / 16, ms->y / 16);
            void vga_refresh_screen(void);
            vga_refresh_screen();
        }
    }
}

void console_copy_selection(void) {
    mouse_state_t* ms = get_mouse_state();
    if (!ms || drag_start_x == -1) return;

    int x1 = drag_start_x / 16;
    int y1 = drag_start_y / 16;
    int x2 = ms->x / 16;
    int y2 = ms->y / 16;

    /* Normalize */
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }

    /* Extract text from terminal buffer */
    char export_buf[1024];
    int e_idx = 0;
    extern char terminal_buffer[40][80];

    for (int r = y1; r <= y2 && r < 40; r++) {
        for (int c = x1; c <= x2 && c < 80; c++) {
            char ch = terminal_buffer[r][c];
            if (ch >= 32 && ch < 127 && e_idx < 1022) {
                export_buf[e_idx++] = ch;
            }
        }
        if (e_idx < 1022) export_buf[e_idx++] = '\n';
    }
    export_buf[e_idx] = '\0';

    if (e_idx > 0) {
        void* s = str_create(export_buf);
        void rsl_copy(void* str);
        rsl_copy(s);
        void release(void* obj);
        release(s);
        vga_print("\n[CLIPBOARD] Copied selection to system clipboard.\n");
    }

    drag_start_x = -1;
    drag_start_y = -1;
    void vga_set_selection(int x1, int y1, int x2, int y2);
    vga_set_selection(-1, -1, -1, -1);
    void vga_refresh_screen(void);
    vga_refresh_screen();
}

char get_char(void) {
    while (1) {
        hw_poll();
        char c = console_pop_char();
        if (c) return c;

        /* Sovereign Active-Relay: Yield while waiting for input */
        task_t* current = get_current_task();
        if (current) {
            current->state = TASK_INPUT_WAIT;
            scheduler_force_task(1);
        }
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
