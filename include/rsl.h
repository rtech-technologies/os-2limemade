#ifndef RSL_H
#define RSL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* RSL Binary Header (Global OS Header) */
typedef struct {
    uint8_t magic[4];       /* "RSL1" */
    uint64_t entry_offset;  /* Offset from start of binary to entry point */
    uint64_t stack_size;    /* Minimum stack size required */
    uint32_t flags;         /* Binary flags (0: Pure, 1: GUI-aware) */
    uint32_t reserved;
} rsl_header_t;

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
void rsl_exit(void);
void rsl_shell_in(void);
void rsl_win_in(void);

/* Inline Syscall Wrappers for RSL Binaries */
#ifdef RSL_BINARY_MODE
static inline void rsl_print(const char* s) {
    __asm__ volatile ("mov $0, %%rax; mov %0, %%rdi; int $0x03" : : "r"(s) : "rax", "rdi");
}

static inline void* rsl_input(const char* prompt) {
    void* ret;
    __asm__ volatile ("mov $3, %%rax; mov %1, %%rdi; int $0x03; mov %%rax, %0" : "=r"(ret) : "r"(prompt) : "rax", "rdi");
    return ret;
}

static inline void rsl_set_color(uint64_t fg, uint64_t bg) {
    __asm__ volatile ("mov $2, %%rax; mov %0, %%rdi; mov %1, %%rsi; int $0x03" : : "r"(fg), "r"(bg) : "rax", "rdi", "rsi");
}

static inline void rsl_release(void* ptr) {
    __asm__ volatile ("mov $5, %%rax; mov %0, %%rdi; int $0x03" : : "r"(ptr) : "rax", "rdi");
}
#endif

#endif /* RSL_H */
