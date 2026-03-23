#include <kernel/libs/services.h>
#include <stdint.h>
#include <stddef.h>

/* I/O Port Helper */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Serial COM1 (0x3F8) Initialization */
#define SERIAL_COM1 0x3F8

static void serial_init(void) {
    outb(SERIAL_COM1 + 1, 0x00);    /* Disable interrupts */
    outb(SERIAL_COM1 + 3, 0x80);    /* Enable DLAB (set baud rate divisor) */
    outb(SERIAL_COM1 + 0, 0x03);    /* Set divisor to 3 (38400 baud) */
    outb(SERIAL_COM1 + 1, 0x00);
    outb(SERIAL_COM1 + 3, 0x03);    /* 8 bits, no parity, one stop bit */
    outb(SERIAL_COM1 + 2, 0xC7);    /* Enable FIFO, clear them, with 14-byte threshold */
    outb(SERIAL_COM1 + 4, 0x0B);    /* IRQs enabled, RTS/DSR set */
}

static int is_transmit_empty(void) {
    return inb(SERIAL_COM1 + 5) & 0x20;
}

void serial_write_char(char c) {
    while (is_transmit_empty() == 0);
    outb(SERIAL_COM1, c);
}

void serial_write_str(const char* s) {
    for (int i = 0; s[i] != '\0'; i++) {
        serial_write_char(s[i]);
    }
}

/* VGA Legacy Fallback (0xB8000) */
#define VGA_ADDR 0xffffffff800b8000ULL /* Mapped into High-Half */
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static uint16_t* vga_buffer = (uint16_t*)VGA_ADDR;
static int vga_cursor_x = 0;
static int vga_cursor_y = 0;

void vga_clear(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = (uint16_t)' ' | (0x07 << 8);
        }
    }
}

void vga_write_char(char c, uint8_t color) {
    if (c == '\n') {
        vga_cursor_x = 0;
        vga_cursor_y++;
    } else {
        vga_buffer[vga_cursor_y * VGA_WIDTH + vga_cursor_x] = (uint16_t)c | ((uint16_t)color << 8);
        vga_cursor_x++;
    }

    if (vga_cursor_x >= VGA_WIDTH) {
        vga_cursor_x = 0;
        vga_cursor_y++;
    }

    if (vga_cursor_y >= VGA_HEIGHT) {
        /* Simple scroll */
        for (int y = 1; y < VGA_HEIGHT; y++) {
            for (int x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[(y - 1) * VGA_WIDTH + x] = vga_buffer[y * VGA_WIDTH + x];
            }
        }
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (uint16_t)' ' | (0x07 << 8);
        }
        vga_cursor_y = VGA_HEIGHT - 1;
    }
}

void vga_serial_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        serial_init();
        serial_write_str("[INIT] Serial initialized.\n");
        vga_clear();
        serial_write_str("[INIT] VGA cleared.\n");
    }
}
