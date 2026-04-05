#ifndef RSL_H
#define RSL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ARC Memory Management */
void retain(void* ptr);
void release(void* ptr);

/* String API (Pythonic) */
void* str_create(const char* cstr);
void* str_concat(void* s1, void* s2);
bool str_is_empty(void* str);
bool str_match(void* str, const char* pattern);
size_t str_len(void* str);
const char* str_to_cstr(void* str);

/* Console API */
typedef enum {
    BLACK = 0, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LIGHT_GRAY,
    DARK_GRAY, LIGHT_BLUE, LIGHT_GREEN, LIGHT_CYAN, LIGHT_RED, PINK, YELLOW, WHITE
} color_t;

void set_color(color_t fg, color_t bg);
void* input(const char* prompt);
void print(const char* s);

/* File System API (RSL Wrappers) */
void rsl_ls(void* path);
void rsl_cat(void* path);
void rsl_write(void* path, void* content);
void rsl_cd(void* path);
void rsl_mkdir(void* path);
void rsl_rmdir(void* path);
bool rsl_exists(void* path);
void rsl_mount(void* path);
void rsl_format(void* path);
void rsl_stamp(void* path);
bool rsl_safe_mode(void);
void rsl_draw_rrif(void* path, int x, int y);
void rsl_scan(void);
void rsl_eject(void* path);
void rsl_copy(void* str);
void* rsl_paste(void);
void rsl_settings(void);
void rsl_debug_dump(void);

#endif /* RSL_H */
