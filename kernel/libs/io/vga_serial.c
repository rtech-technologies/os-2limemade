#include <ksdk/core/ksdk.h>
#include <include/config.h>
#include <limine.h>
#define SERIAL_PORT 0x3F8
#define ROWS 24
#define COLS 80
#define SCALE 2
static inline void outb(uint16_t port, uint8_t val) { __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port)); }
static inline uint8_t inb(uint16_t port) { uint8_t ret; __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port)); return ret; }
static void serial_init(void) {
    outb(SERIAL_PORT + 1, 0x00); outb(SERIAL_PORT + 3, 0x80); outb(SERIAL_PORT + 0, 0x03);
    outb(SERIAL_PORT + 1, 0x00); outb(SERIAL_PORT + 3, 0x03); outb(SERIAL_PORT + 2, 0xC7);
    outb(SERIAL_PORT + 4, 0x0B);
}
void serial_write_char(char c) { while (!(inb(SERIAL_PORT + 5) & 0x20)); outb(SERIAL_PORT, c); }
void serial_write_str(const char* s) { while (*s) serial_write_char(*s++); }
int serial_received(void) { return inb(SERIAL_PORT + 5) & 1; }
char serial_read_char(void) { while (!serial_received()); return inb(SERIAL_PORT); }
static const uint8_t font8x8_basic[128][8] = { [0x20]={0,0,0,0,0,0,0,0}, [0x21]={0x18,0x18,0x18,0x18,0,0,0x18,0}, [0x41]={0x18,0x3C,0x66,0x7E,0x66,0x66,0x66,0} /* Simplified for SDK logic demo */ };
static uint32_t vga_colors[] = { 0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA, 0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF };
static struct limine_framebuffer* global_fb = NULL;
void draw_char_at(char c, int x, int y, uint8_t attr) {
    if (!global_fb || (uint8_t)c >= 128 || x < 0 || x >= COLS || y < 0 || y >= ROWS) return;
    if ((y + 1) * 8 * SCALE > (int)global_fb->height) return;
    uint32_t fg = vga_colors[attr & 0x0F], bg = vga_colors[attr >> 4];
    const uint8_t* glyph = font8x8_basic[(uint8_t)c];
    for (int i=0; i<8; i++) for (int j=0; j<8; j++) {
        uint32_t color = (glyph[i] & (1 << (7 - j))) ? fg : bg;
        for (int sy=0; sy<SCALE; sy++) for (int sx=0; sx<SCALE; sx++) {
            int py = y*8*SCALE + i*SCALE + sy, px = x*8*SCALE + j*SCALE + sx;
            if (px >= 0 && (uint64_t)px < global_fb->width && py >= 0 && (uint64_t)py < global_fb->height) { ((uint32_t*)global_fb->address)[(py*global_fb->pitch/4) + px] = color; }
        }
    }
}
void vga_clear(void) { if (!global_fb) return; for (uint64_t i=0; i<global_fb->height * global_fb->pitch / 4; i++) ((uint32_t*)global_fb->address)[i] = 0; }
void vga_write_char(char c, uint8_t attr) {
    static int cursor_x = 0, cursor_y = 0;
    if (c == '\b') { if (cursor_x > 0) cursor_x--; draw_char_at(' ', cursor_x, cursor_y, attr); return; }
    if (c == '\n') { cursor_x = 0; cursor_y++; }
    else { draw_char_at(c, cursor_x, cursor_y, attr); cursor_x++; if (cursor_x >= COLS) { cursor_x = 0; cursor_y++; } }
    if (cursor_y >= ROWS) cursor_y = ROWS - 1;
}
void telemetry_update(int id, const char* s) { (void)id; (void)s; }
void draw_pixel(int x, int y, uint32_t c) { if(global_fb && x>=0 && (uint64_t)x<global_fb->width && y>=0 && (uint64_t)y<global_fb->height) ((uint32_t*)global_fb->address)[(y*global_fb->pitch/4)+x]=c; }
void vga_set_cursor(int x, int y) { (void)x; (void)y; }
void vga_pulse_cursor(void) {}
void vga_serial_service(kernel_event_t event) {
    if (event == EVENT_INIT) { serial_init(); struct limine_framebuffer_response* resp = get_framebuffer(); if (resp && resp->framebuffer_count > 0) global_fb = resp->framebuffers[0]; vga_clear(); }
}
