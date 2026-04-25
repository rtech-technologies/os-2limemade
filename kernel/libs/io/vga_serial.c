#include <kernel/libs/core/services.h>
#include <kernel/unice64/task.h>
#include <include/config.h>
#include <limine.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* I/O Port Helper */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Serial Port Initialization */
#define SERIAL_PORT CONFIG_SERIAL_PORT

void serial_init(void) {
    outb(SERIAL_PORT + 1, 0x00);
    outb(SERIAL_PORT + 3, 0x80);
    outb(SERIAL_PORT + 0, 0x03);
    outb(SERIAL_PORT + 1, 0x00);
    outb(SERIAL_PORT + 3, 0x03);
    outb(SERIAL_PORT + 2, 0xC7);
    outb(SERIAL_PORT + 4, 0x0B);
}

static int is_transmit_empty(void) {
    return inb(SERIAL_PORT + 5) & 0x20;
}

void pit_wait_ms(uint32_t ms);

void serial_write_char(char c) {
    while (is_transmit_empty() == 0) {
        __asm__ volatile ("pause");
    }
    outb(SERIAL_PORT, c);
}

int serial_received(void) {
    return inb(SERIAL_PORT + 5) & 1;
}

char serial_read_char(void) {
    while (serial_received() == 0) {
        __asm__ volatile ("pause");
    }
    return inb(SERIAL_PORT);
}

void serial_write_str(const char* s) {
    for (int i = 0; s[i] != '\0'; i++) {
        serial_write_char(s[i]);
    }
}

uint64_t get_hhdm_offset(void);
struct limine_framebuffer_response* get_framebuffer(void);
extern bool g_vga_silent;

/* Complete 8x8 Font (ASCII 0-127) */
static const uint8_t font8x8_basic[128][8] = {
    [0x20] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // space
    [0x21] = { 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x18, 0x00 }, // !
    [0x22] = { 0x6C, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00 }, // "
    [0x23] = { 0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00 }, // #
    [0x24] = { 0x18, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x18, 0x00 }, // $
    [0x25] = { 0x00, 0x62, 0x66, 0x0C, 0x18, 0x33, 0x23, 0x00 }, // %
    [0x26] = { 0x38, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0x76, 0x00 }, // &
    [0x27] = { 0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00 }, // '
    [0x28] = { 0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00 }, // (
    [0x29] = { 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00 }, // )
    [0x2A] = { 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00 }, // *
    [0x2B] = { 0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00 }, // +
    [0x2C] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30 }, // ,
    [0x2D] = { 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00 }, // -
    [0x2E] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00 }, // .
    [0x2F] = { 0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x00 }, // /
    [0x30] = { 0x3C, 0x66, 0x6E, 0x7E, 0x76, 0x66, 0x3C, 0x00 }, // 0
    [0x31] = { 0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00 }, // 1
    [0x32] = { 0x3C, 0x66, 0x06, 0x3C, 0x60, 0x66, 0x7E, 0x00 }, // 2
    [0x33] = { 0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00 }, // 3
    [0x34] = { 0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00 }, // 4
    [0x35] = { 0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00 }, // 5
    [0x36] = { 0x3C, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x3C, 0x00 }, // 6
    [0x37] = { 0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00 }, // 7
    [0x38] = { 0x3C, 0x66, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00 }, // 8
    [0x39] = { 0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00 }, // 9
    [0x3A] = { 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00 }, // :
    [0x3B] = { 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00 }, // ;
    [0x3C] = { 0x0C, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0C, 0x00 }, // <
    [0x3D] = { 0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00 }, // =
    [0x3E] = { 0x30, 0x18, 0x0C, 0x06, 0x0C, 0x18, 0x30, 0x00 }, // >
    [0x3F] = { 0x3C, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00 }, // ?
    [0x40] = { 0x3C, 0x66, 0x6E, 0x6A, 0x60, 0x62, 0x3C, 0x00 }, // @
    [0x41] = { 0x18, 0x3C, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00 }, // A
    [0x42] = { 0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00 }, // B
    [0x43] = { 0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00 }, // C
    [0x44] = { 0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00 }, // D
    [0x45] = { 0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x7E, 0x00 }, // E
    [0x46] = { 0x7E, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00 }, // F
    [0x47] = { 0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00 }, // G
    [0x48] = { 0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00 }, // H
    [0x49] = { 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00 }, // I
    [0x4A] = { 0x06, 0x06, 0x06, 0x06, 0x06, 0x66, 0x3C, 0x00 }, // J
    [0x4B] = { 0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00 }, // K
    [0x4C] = { 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00 }, // L
    [0x4D] = { 0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00 }, // M
    [0x4E] = { 0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00 }, // N
    [0x4F] = { 0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00 }, // O
    [0x50] = { 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00 }, // P
    [0x51] = { 0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x0E, 0x00 }, // Q
    [0x52] = { 0x7C, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x00 }, // R
    [0x53] = { 0x3C, 0x66, 0x30, 0x1C, 0x06, 0x66, 0x3C, 0x00 }, // S
    [0x54] = { 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00 }, // T
    [0x55] = { 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00 }, // U
    [0x56] = { 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00 }, // V
    [0x57] = { 0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00 }, // W
    [0x58] = { 0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00 }, // X
    [0x59] = { 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x00 }, // Y
    [0x5A] = { 0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00 }, // Z
    [0x5B] = { 0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00 }, // [
    [0x5C] = { 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00 }, // Backslash
    [0x5D] = { 0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00 }, // ]
    [0x5E] = { 0x18, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00 }, // ^
    [0x5F] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF }, // _
    [0x60] = { 0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00 }, // `
    [0x61] = { 0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E, 0x00 }, // a
    [0x62] = { 0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00 }, // b
    [0x63] = { 0x00, 0x00, 0x3C, 0x60, 0x60, 0x66, 0x3C, 0x00 }, // c
    [0x64] = { 0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00 }, // d
    [0x65] = { 0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00 }, // e
    [0x66] = { 0x1C, 0x30, 0x78, 0x30, 0x30, 0x30, 0x30, 0x00 }, // f
    [0x67] = { 0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C }, // g
    [0x68] = { 0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00 }, // h
    [0x69] = { 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00 }, // i
    [0x6A] = { 0x0C, 0x00, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x38 }, // j
    [0x6B] = { 0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00 }, // k
    [0x6C] = { 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00 }, // l
    [0x6D] = { 0x00, 0x00, 0x66, 0x7F, 0x7F, 0x6B, 0x63, 0x00 }, // m
    [0x6E] = { 0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00 }, // n
    [0x6F] = { 0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00 }, // o
    [0x70] = { 0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60 }, // p
    [0x71] = { 0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06 }, // q
    [0x72] = { 0x00, 0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x00 }, // r
    [0x73] = { 0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00 }, // s
    [0x74] = { 0x30, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x1C, 0x00 }, // t
    [0x75] = { 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00 }, // u
    [0x76] = { 0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00 }, // v
    [0x77] = { 0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00 }, // w
    [0x78] = { 0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00 }, // x
    [0x79] = { 0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x3C }, // y
    [0x7A] = { 0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00 }, // z
    [0x7B] = { 0x0C, 0x18, 0x18, 0x30, 0x18, 0x18, 0x0C, 0x00 }, // {
    [0x7C] = { 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00 }, // |
    [0x7D] = { 0x30, 0x18, 0x18, 0x0C, 0x18, 0x18, 0x30, 0x00 }, // }
    [0x7E] = { 0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // ~
};

static uint32_t vga_colors[] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

static int cursor_x = 0;
static int cursor_y = 0;
static bool cursor_visible = true;

#define SCALE 2
#define TERM_COLS 80
#define TERM_ROWS 40

char terminal_buffer[TERM_ROWS][TERM_COLS];
uint8_t terminal_attr[TERM_ROWS][TERM_COLS];

static int selection_x1 = -1, selection_y1 = -1;
static int selection_x2 = -1, selection_y2 = -1;

static uint32_t mouse_back_buffer[16 * 16];
static int last_mouse_x = -1;
static int last_mouse_y = -1;

static const uint16_t mouse_cursor_bitmap[16] = {
    0b1000000000000000,
    0b1100000000000000,
    0b1110000000000000,
    0b1111000000000000,
    0b1111100000000000,
    0b1111110000000000,
    0b1111111000000000,
    0b1111111100000000,
    0b1111111110000000,
    0b1111110000000000,
    0b1101110000000000,
    0b1000111000000000,
    0b0000111000000000,
    0b0000011100000000,
    0b0000011100000000,
    0b0000000000000000
};

static struct limine_framebuffer* global_fb = NULL;

void draw_pixel(int x, int y, uint32_t color) {
    if (!global_fb) return;
    struct limine_framebuffer* fb = global_fb;
    if (x < 0 || (uint64_t)x >= fb->width || y < 0 || (uint64_t)y >= fb->height) return;
    uint32_t* pixel = (uint32_t*)(fb->address + y * fb->pitch + x * 4);
    *pixel = color;
}

uint32_t get_pixel(int x, int y) {
    if (!global_fb) return 0;
    struct limine_framebuffer* fb = global_fb;
    if (x < 0 || (uint64_t)x >= fb->width || y < 0 || (uint64_t)y >= fb->height) return 0;
    uint32_t* pixel = (uint32_t*)(fb->address + y * fb->pitch + x * 4);
    return *pixel;
}

void vga_erase_mouse(void) {
    if (last_mouse_x == -1) return;
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            draw_pixel(last_mouse_x + j, last_mouse_y + i, mouse_back_buffer[i * 16 + j]);
        }
    }
}

void vga_draw_mouse(int x, int y) {
    if (!global_fb) return;
    vga_erase_mouse();

    last_mouse_x = x;
    last_mouse_y = y;

    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            mouse_back_buffer[i * 16 + j] = get_pixel(x + j, y + i);
            if (mouse_cursor_bitmap[i] & (1 << (15 - j))) {
                draw_pixel(x + j, y + i, 0xFF00FF); /* Hot Pink */
            }
        }
    }
}

void draw_char_pixel(char c, int px, int py, uint32_t fg, uint32_t bg) {
    if (!global_fb) return;
    struct limine_framebuffer* fb = global_fb;
    if ((uint8_t)c >= 128) return;

    const uint8_t* glyph = font8x8_basic[(uint8_t)c];
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            uint32_t color = (glyph[i] & (1 << (7 - j))) ? fg : bg;
            for (int sy = 0; sy < SCALE; sy++) {
                for (int sx = 0; sx < SCALE; sx++) {
                    int final_x = px + (j * SCALE) + sx;
                    int final_y = py + (i * SCALE) + sy;
                    if (final_x >= 0 && (uint64_t)final_x < fb->width && final_y >= 0 && (uint64_t)final_y < fb->height) {
                        uint32_t* pixel = (uint32_t*)(fb->address + final_y * fb->pitch + final_x * 4);
                        *pixel = color;
                    }
                }
            }
        }
    }
}

void draw_char(char c, int x, int y, uint32_t fg, uint32_t bg) {
    draw_char_pixel(c, x * 8 * SCALE, y * 8 * SCALE, fg, bg);
}

void vga_set_selection(int x1, int y1, int x2, int y2) {
    selection_x1 = x1; selection_y1 = y1;
    selection_x2 = x2; selection_y2 = y2;
}

void vga_write_char(char c, uint8_t color_attr) {
    /*
     * Mirror VGA to Serial:
     * Every character passed to the VGA console is transmitted to Serial COM1.
     */
    if (c == '\b') {
        serial_write_char('\b');
        serial_write_char(' ');
        serial_write_char('\b');
    } else {
        serial_write_char(c);
    }

    if (g_vga_silent) return;

    vga_erase_mouse();

    /* Ignore non-printable gibberish except for key control codes */
    if ((uint8_t)c < 32 && c != '\n' && c != '\r' && c != '\b' && c != '\t') return;
    if ((uint8_t)c >= 127) return;

    if (!global_fb) return;
    struct limine_framebuffer* fb = global_fb;
    int char_width = 8 * SCALE;
    int max_cols = fb->width / char_width;

    uint32_t fg = vga_colors[color_attr & 0x0F];
    uint32_t bg = vga_colors[(color_attr >> 4) & 0x0F];

    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = max_cols - 1;
        }
        draw_char(' ', cursor_x, cursor_y, fg, bg);
        if (cursor_y < TERM_ROWS && cursor_x < TERM_COLS) {
            terminal_buffer[cursor_y][cursor_x] = ' ';
            terminal_attr[cursor_y][cursor_x] = color_attr;
        }
    } else {
        /* Dynamic line wrapping based on framebuffer width */
        if (cursor_x >= max_cols) {
            cursor_x = 0;
            cursor_y++;
        }

        bool selected = false;
        if (selection_x1 != -1) {
            int x1 = selection_x1, y1 = selection_y1;
            int x2 = selection_x2, y2 = selection_y2;
            if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
            if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
            if (cursor_y >= y1 && cursor_y <= y2 && cursor_x >= x1 && cursor_x <= x2) selected = true;
        }

        if (selected) draw_char(c, cursor_x, cursor_y, bg, fg); /* Invert */
        else draw_char(c, cursor_x, cursor_y, fg, bg);

        if (cursor_y < TERM_ROWS && cursor_x < TERM_COLS) {
            terminal_buffer[cursor_y][cursor_x] = c;
            terminal_attr[cursor_y][cursor_x] = color_attr;
        }
        cursor_x++;
    }

    /* Vertical Scrolling Logic */
    struct limine_framebuffer_response* fb_resp = get_framebuffer();
    if (!fb_resp || fb_resp->framebuffer_count == 0) return;

    int char_height = 8 * SCALE;
    /* OSx2: Cap terminal based on user configuration */
    /* Leave room for telemetry row */
    int max_rows = (fb->height / char_height) - 1;
#ifdef CONFIG_TERMINAL_ROWS
    if (max_rows > CONFIG_TERMINAL_ROWS) max_rows = CONFIG_TERMINAL_ROWS;
#else
    if (max_rows > 20) max_rows = 20;
#endif

    if (cursor_y >= max_rows) {
        /* Scroll terminal buffer */
        for (int row = 0; row < max_rows - 1; row++) {
            for (int col = 0; col < TERM_COLS; col++) {
                terminal_buffer[row][col] = terminal_buffer[row + 1][col];
                terminal_attr[row][col] = terminal_attr[row + 1][col];
            }
        }
        /* Clear bottom row of buffer */
        for (int col = 0; col < TERM_COLS; col++) {
            terminal_buffer[max_rows - 1][col] = ' ';
            terminal_attr[max_rows - 1][col] = 0x07;
        }

        /* Move all rows up by one char_height */
        uint32_t* fb_ptr = (uint32_t*)fb->address;
        size_t row_pixels = fb->pitch / 4;
        size_t scroll_size = (max_rows - 1) * char_height * row_pixels;
        size_t offset = char_height * row_pixels;

        for (size_t i = 0; i < scroll_size; i++) {
            fb_ptr[i] = fb_ptr[i + offset];
        }

        /* Clear the bottom row */
        size_t bottom_start = (max_rows - 1) * char_height * row_pixels;
        size_t bottom_size = char_height * row_pixels;
        for (size_t i = 0; i < bottom_size; i++) {
            fb_ptr[bottom_start + i] = 0x000000;
        }

        cursor_y = max_rows - 1;
    }

    if (last_mouse_x != -1) vga_draw_mouse(last_mouse_x, last_mouse_y);
}

void vga_refresh_screen(void) {
    if (!global_fb) return;
    vga_erase_mouse();

    struct limine_framebuffer* fb = global_fb;
    int char_width = 8 * SCALE;
    int max_cols = fb->width / char_width;
    int char_height = 8 * SCALE;
    int max_rows = (fb->height / char_height) - 1;

    for (int r = 0; r < max_rows; r++) {
        for (int c = 0; c < max_cols && c < TERM_COLS; c++) {
            char ch = terminal_buffer[r][c];
            uint8_t attr = terminal_attr[r][c];
            uint32_t fg = vga_colors[attr & 0x0F];
            uint32_t bg = vga_colors[(attr >> 4) & 0x0F];

            bool selected = false;
            if (selection_x1 != -1) {
                int x1 = selection_x1, y1 = selection_y1;
                int x2 = selection_x2, y2 = selection_y2;
                if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
                if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
                if (r >= y1 && r <= y2 && c >= x1 && c <= x2) selected = true;
            }

            if (selected) draw_char(ch ? ch : ' ', c, r, bg, fg);
            else draw_char(ch ? ch : ' ', c, r, fg, bg);
        }
    }

    if (last_mouse_x != -1) vga_draw_mouse(last_mouse_x, last_mouse_y);
}

/* Sovereign Telemetry Monitor (Bottom Row) */
void telemetry_update(int task_id, const char* status) {
    if (!global_fb) return;
    struct limine_framebuffer* fb = global_fb;
    int char_height = 8 * SCALE;
    int char_width = 8 * SCALE;
    int bottom_row = (fb->height / char_height) - 1;
    int max_cols = fb->width / char_width;

    /* Draw a separator line above telemetry */
    uint32_t sep_color = 0x555555;
    uint32_t* fb_ptr = (uint32_t*)fb->address;
    int line_y = bottom_row * char_height - 2;
    for (uint32_t x = 0; x < fb->width; x++) {
        fb_ptr[line_y * (fb->pitch / 4) + x] = sep_color;
    }

    /* Clear the telemetry row */
    for (int x = 0; x < max_cols; x++) {
        draw_char(' ', x, bottom_row, 0x000000, 0x000000);
    }

    /* Print Status: [T:ID] HEARTBEAT STATUS */
    char buf[64];
    /* Simplified snprintf equivalent */
    int i = 0;
    buf[i++] = '['; buf[i++] = 'T'; buf[i++] = ':';
    buf[i++] = (task_id % 10) + '0';
    buf[i++] = ']'; buf[i++] = ' ';

    static int heartbeat = 0;
    const char* hb_chars = "|/-\\";
    buf[i++] = hb_chars[heartbeat++ % 4];
    buf[i++] = ' ';

    /* INPUT_WAIT Override: Display tag if in waiting state */
    task_t* cur = get_task_by_id(task_id);
    if (cur && cur->state == TASK_INPUT_WAIT) {
        const char* tag = "INPUT_WAIT ";
        int j = 0; while (tag[j]) buf[i++] = tag[j++];
    }

    int k = 0;
    while (status[k] && i < 60) buf[i++] = status[k++];

    /* Mouse Telemetry: [X:pos Y:pos] */
    #include <include/mouse.h>
    mouse_state_t* ms = get_mouse_state();
    if (ms && ms->active) {
        buf[i++] = ' '; buf[i++] = '['; buf[i++] = 'M'; buf[i++] = ':';
        /* Very simplified integer to string for telemetry */
        buf[i++] = (ms->x / 100 % 10) + '0';
        buf[i++] = (ms->x / 10 % 10) + '0';
        buf[i++] = (ms->x % 10) + '0';
        buf[i++] = ',';
        buf[i++] = (ms->y / 100 % 10) + '0';
        buf[i++] = (ms->y / 10 % 10) + '0';
        buf[i++] = (ms->y % 10) + '0';
        buf[i++] = ']';
    }

    buf[i] = '\0';

    /* Draw at bottom left in Emerald (0x00FF88) */
    for (int j = 0; j < i; j++) {
        draw_char(buf[j], j, bottom_row, 0x00FF88, 0x000000);
    }
}

void vga_clear(void) {
    if (!global_fb) return;
    struct limine_framebuffer* fb = global_fb;

    for (uint64_t i = 0; i < fb->height * fb->pitch / 4; i++) {
        ((uint32_t*)fb->address)[i] = 0x000000;
    }
    cursor_x = 0;
    cursor_y = 0;

    /* Clear terminal buffer */
    for (int r = 0; r < TERM_ROWS; r++) {
        for (int c = 0; c < TERM_COLS; c++) {
            terminal_buffer[r][c] = ' ';
            terminal_attr[r][c] = 0x07;
        }
    }
}

void vga_set_cursor(int x, int y) {
    cursor_x = x;
    cursor_y = y;
}

void vga_print(const char* fmt, ...);

void vga_print_logo(void) {
    vga_set_cursor(0, 0);
    vga_print("\n\n");
    vga_print("  _____ _______ ______ _____ _    _ \n");
    vga_print(" |  __ \\__   __|  ____/ ____| |  | |\n");
    vga_print(" | |__) | | |  | |__ | |    | |__| |\n");
    vga_print(" |  _  /  | |  |  __|| |    |  __  |\n");
    vga_print(" | | \\ \\  | |  | |___| |____| |  | |\n");
    vga_print(" |_|  \\_\\ |_|  |______\\_____|_|  |_|\n");
    vga_print("\n [ RTECH SOVEREIGN ] MECHANICAL TRUTH \n\n");
}

void vga_pulse_cursor(void) {
    if (!global_fb) return;
    static uint64_t last_pulse = 0;
    extern uint64_t get_system_ticks(void);
    uint64_t now = get_system_ticks();

    if (now - last_pulse > 500) {
        last_pulse = now;
        cursor_visible = !cursor_visible;
        /* Full Pure Green Pulse for Text Cursor visibility */
        uint32_t color = cursor_visible ? 0x00FF00 : 0x000000;
        /* Draw 8x16 block cursor */
        for (int i = 0; i < 8 * SCALE; i++) {
            for (int j = 0; j < 8 * SCALE; j++) {
                draw_pixel(cursor_x * 8 * SCALE + j, cursor_y * 8 * SCALE + i, color);
            }
        }
    }
}

void serial_print_hex(const char* label, uint16_t val) {
    serial_write_str(label);
    serial_write_str("0x");
    const char* hex = "0123456789ABCDEF";
    serial_write_char(hex[(val >> 12) & 0xF]);
    serial_write_char(hex[(val >> 8) & 0xF]);
    serial_write_char(hex[(val >> 4) & 0xF]);
    serial_write_char(hex[val & 0xF]);
    serial_write_char('\n');
}

void serial_print_hex32(const char* label, uint32_t val) {
    serial_write_str(label);
    serial_write_str("0x");
    const char* hex = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) {
        serial_write_char(hex[(val >> (i * 4)) & 0xF]);
    }
    serial_write_char('\n');
}

void vga_serial_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        serial_init();

        struct limine_framebuffer_response* fb_resp = get_framebuffer();
        if (fb_resp && fb_resp->framebuffer_count > 0) {
            global_fb = fb_resp->framebuffers[0];
            serial_write_str("[INIT] GOP Framebuffer initialized.\n");
        } else {
            serial_write_str("[WARN] GOP Framebuffer not found, console output disabled.\n");
        }

        serial_write_str("[INIT] Serial and VGA Mirroring active.\n");
        vga_clear();
    }
}
