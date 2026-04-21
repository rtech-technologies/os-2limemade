#include <ksdk/core/ksdk.h>

static uint8_t current_color_val = 0x07;
static inline uint8_t inb(uint16_t port) { uint8_t ret; __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port)); return ret; }
void set_color(color_t fg, color_t bg) { current_color_val = ((uint8_t)bg << 4) | ((uint8_t)fg & 0x0F); }
void print(const char* s) { if (!s) return; for (int i = 0; s[i] != '\0'; i++) vga_write_char(s[i], current_color_val); sys_yield(); }
static void print_num(uint32_t n, int base) {
    char buf[32]; int i = 0; if (n == 0) { vga_write_char('0', current_color_val); return; }
    const char* digits = "0123456789ABCDEF"; while (n > 0) { buf[i++] = digits[n % base]; n /= base; }
    while (i > 0) vga_write_char(buf[--i], current_color_val);
}
void vga_print(const char* fmt, ...) {
    __builtin_va_list args; __builtin_va_start(args, fmt);
    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] == '%' && fmt[i+1] != '\0') {
            i++; if (fmt[i] == 'd') print_num(__builtin_va_arg(args, int), 10);
            else if (fmt[i] == 'x') print_num(__builtin_va_arg(args, uint32_t), 16);
            else if (fmt[i] == 's') print(__builtin_va_arg(args, char*));
        } else vga_write_char(fmt[i], current_color_val);
    }
    __builtin_va_end(args);
}
static char scancode_map[128] = { 0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ' };
static char shift_scancode_map[128] = { 0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ' };
static bool shift_pressed = false;
#define INPUT_BUFFER_SIZE 256
static char input_buffer[INPUT_BUFFER_SIZE];
static int input_head = 0, input_tail = 0;
void console_push_char(char c) { int next = (input_head + 1) % INPUT_BUFFER_SIZE; if (next != input_tail) { input_buffer[input_head] = c; input_head = next; } }
char console_pop_char(void) { if (input_head == input_tail) return 0; char c = input_buffer[input_tail]; input_tail = (input_tail + 1) % INPUT_BUFFER_SIZE; return c; }
char get_char(void) {
    while (1) {
        char uc = console_pop_char(); if (uc) return uc;
        uint8_t status = inb(0x64);
        if (status & 1) {
            uint8_t scancode = inb(0x60); if (status & 0x20) continue;
            if (scancode == 0x2A || scancode == 0x36) { shift_pressed = true; continue; }
            if (scancode == 0xAA || scancode == 0xB6) { shift_pressed = false; continue; }
            if (scancode == 0x9C) return -1; if (scancode & 0x80) continue;
            if (scancode == 0x01) return 27;
            if (scancode < 128) { char c = shift_pressed ? shift_scancode_map[scancode] : scancode_map[scancode]; if (c) return c; }
        }
        if (serial_received()) { char c = serial_read_char(); if (c == 27 || c == '\n' || c == '\r' || c == '\b' || (c >= 32 && c <= 126)) return c; }
        task_t* current = get_current_task(); if (current) current->state = TASK_WAITING;
        sys_yield(); if (current) current->state = TASK_RUNNING;
        __asm__ volatile ("pause");
    }
}
void* input(const char* prompt) {
    if (prompt) print(prompt);
    char buffer[128]; int idx = 0;
    while (idx < 127) {
        char c = get_char(); if (c == (char)-1) continue;
        if (c == 27) { vga_write_char('^', current_color_val); vga_write_char('C', current_color_val); vga_write_char('\n', current_color_val); return NULL; }
        if (c == '\n' || c == '\r') { vga_write_char('\n', current_color_val); break; }
        else if (c == '\b') { if (idx > 0) { idx--; vga_write_char('\b', current_color_val); } }
        else { buffer[idx++] = c; vga_write_char(c, current_color_val); }
    }
    buffer[idx] = '\0'; return str_create(buffer);
}
