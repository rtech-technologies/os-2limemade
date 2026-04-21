#ifndef KSDK_VGA_H
#define KSDK_VGA_H
#include <ksdk/core/types.h>

void vga_print(const char* fmt, ...);
void draw_char_at(char c, int x, int y, uint8_t attr);
void draw_pixel(int x, int y, uint32_t color);
void vga_write_char(char c, uint8_t attr);
void console_push_char(char c);
char console_pop_char(void);

void serial_write_char(char c);
void serial_write_str(const char* s);
int serial_received(void);
char serial_read_char(void);

void telemetry_update(int task_id, const char* status);
void vga_clear(void);
void vga_set_cursor(int x, int y);
void vga_pulse_cursor(void);

struct limine_framebuffer_response* get_framebuffer(void);

#endif
