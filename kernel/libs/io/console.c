#include <include/rsl.h>
#include <kernel/unice64/task.h>
#include <stdint.h>
#include <stddef.h>

void vga_write_char(char c, uint8_t color_val);
char serial_read_char(void);
int serial_received(void);

static uint8_t current_color_val = 0x07;
bool g_vga_silent = false;
uint64_t g_kbd_initial_delay = 500;
uint64_t g_kbd_repeat_rate = 50;

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
    for (int i = 0; s[i] != '\0'; i++) vga_write_char(s[i], current_color_val);
}

static void print_num(uint64_t n, int base) {
    char buf[64]; int i = 0;
    if (n == 0) { vga_write_char('0', current_color_val); return; }
    const char* digits = "0123456789ABCDEF";
    while (n > 0) { buf[i++] = digits[n % base]; n /= base; }
    while (i > 0) vga_write_char(buf[--i], current_color_val);
}

void vga_print(const char* fmt, ...) {
    __builtin_va_list args; __builtin_va_start(args, fmt);
    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%' && fmt[i+1] != '\0') {
            i++; bool long_mode = false;
            if (fmt[i] == 'l') { long_mode = true; i++; }
            if (fmt[i] == 'd') {
                uint64_t n = long_mode ? __builtin_va_arg(args, uint64_t) : (uint64_t)__builtin_va_arg(args, int);
                print_num(n, 10);
            } else if (fmt[i] == 'x' || fmt[i] == 'p') {
                uint64_t n = (long_mode || fmt[i] == 'p') ? __builtin_va_arg(args, uint64_t) : (uint64_t)__builtin_va_arg(args, uint32_t);
                if (fmt[i] == 'p') { vga_write_char('0', current_color_val); vga_write_char('x', current_color_val); }
                print_num(n, 16);
            } else if (fmt[i] == 's') {
                char* s = __builtin_va_arg(args, char*);
                if (s) { for (int k = 0; s[k] != '\0'; k++) vga_write_char(s[k], current_color_val); }
                else { const char* n = "(null)"; for (int k = 0; n[k] != '\0'; k++) vga_write_char(n[k], current_color_val); }
            } else if (fmt[i] == 'c') {
                vga_write_char((char)__builtin_va_arg(args, int), current_color_val);
            }
        } else { vga_write_char(fmt[i], current_color_val); }
    }
    __builtin_va_end(args);
}

static char __attribute__((used)) scancode_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',
  '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e', 'r',
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
 '\'', '`',   0, '\\', 'z', 'x', 'c', 'v', 'b', 'n',
  'm', ',', '.', '/',   0, '*', 0, ' ',
};

static char __attribute__((used)) shift_scancode_map[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*',
  '(', ')', '_', '+', '\b', '\t', 'Q', 'W', 'E', 'R',
  'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
 '"', '~',   0, '|', 'Z', 'X', 'C', 'V', 'B', 'N',
  'M', '<', '>', '?',   0, '*', 0, ' ',
};

static bool shift_pressed = false;
bool control_pressed = false;

#define KBD_BUF_SIZE 64
static char console_input_buffer[KBD_BUF_SIZE];
static int input_head = 0; static int input_tail = 0;

void console_push_char(char c) {
    int next = (input_tail + 1) % KBD_BUF_SIZE;
    if (next != input_head) { console_input_buffer[input_tail] = c; input_tail = next; }
}

char console_pop_char(void) {
    if (input_head == input_tail) return 0;
    char c = console_input_buffer[input_head]; input_head = (input_head + 1) % KBD_BUF_SIZE;
    return c;
}

void hw_poll(void) {
    #include <include/config.h>
    uint8_t status = inb(0x64);
    if (status & 1) {
        uint8_t scancode = inb(0x60);
        if (!(status & 0x20)) {
            if (scancode == 0x2A || scancode == 0x36) shift_pressed = true;
            else if (scancode == 0xAA || scancode == 0xB6) shift_pressed = false;
            else if (scancode == 0x1D) control_pressed = true;
            else if (scancode == 0x9D) control_pressed = false;
            else if (!(scancode & 0x80)) {
                if (scancode < 128) {
                    char c = shift_pressed ? shift_scancode_map[scancode] : scancode_map[scancode];
                    if (c) console_push_char(c);
                }
            }
        }
    }
    if (serial_received()) {
        char c = serial_read_char();
        if (c == '\n' || c == '\r' || c == '\b' || c == 27 || (c >= 32 && c <= 126)) console_push_char(c);
    }
}

char get_char(void) {
    while (1) {
        hw_poll(); char c = console_pop_char(); if (c) return c;
        task_t* current = get_current_task();
        if (current) { current->state = TASK_INPUT_WAIT; scheduler_force_task(1); }
        sys_yield(); if (current) current->state = TASK_RUNNING;
        __asm__ volatile ("pause");
    }
}

void* input(const char* prompt) {
    if (prompt) print(prompt);
    char buffer[128]; int idx = 0;
    while (idx < 127) {
        char c = get_char();
        if (c == (char)-1) continue;
        if (c == 27) { vga_write_char('^', 0x07); vga_write_char('C', 0x07); vga_write_char('\n', 0x07); return NULL; }
        if (c == '\n' || c == '\r') { vga_write_char('\n', 0x07); break; }
        else if (c == '\b') { if (idx > 0) { idx--; vga_write_char('\b', 0x07); } }
        else { buffer[idx++] = c; vga_write_char(c, 0x07); }
    }
    buffer[idx] = '\0'; return str_create(buffer);
}
