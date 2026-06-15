#ifndef RSL_H
#define RSL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    BLACK = 0, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, LIGHT_GRAY,
    DARK_GRAY, LIGHT_BLUE, LIGHT_GREEN, LIGHT_CYAN, LIGHT_RED, PINK, YELLOW, WHITE
} color_t;

#ifdef RSL_BINARY_MODE
/*
 * Standalone Mode: Use System Calls (int 0x03)
 */
static inline uint64_t rsl_syscall(uint64_t id, uint64_t a1, uint64_t a2, uint64_t a3) {
    uint64_t ret;
    __asm__ volatile (
        "mov %1, %%rax\n"
        "mov %2, %%rdi\n"
        "mov %3, %%rsi\n"
        "mov %4, %%rdx\n"
        "int $0x03\n"
        "mov %%rax, %0"
        : "=r"(ret)
        : "r"(id), "r"(a1), "r"(a2), "r"(a3)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
    );
    return ret;
}

#define print(s) rsl_syscall(0, (uint64_t)(s), 0, 0)
#define input(p) (void*)rsl_syscall(1, (uint64_t)(p), 0, 0)
#define set_color(f, b) rsl_syscall(2, (uint64_t)(f), (uint64_t)(b), 0)
#define str_create(c) (void*)rsl_syscall(3, (uint64_t)(c), 0, 0)
#define release(p) rsl_syscall(4, (uint64_t)(p), 0, 0)
#define str_match(s, p) (bool)rsl_syscall(5, (uint64_t)(s), (uint64_t)(p), 0)
#define str_concat(s1, s2) (void*)rsl_syscall(6, (uint64_t)(s1), (uint64_t)(s2), 0)
#define str_to_cstr(s) (const char*)rsl_syscall(7, (uint64_t)(s), 0, 0)
#define str_is_empty(s) (bool)rsl_syscall(8, (uint64_t)(s), 0, 0)

/* FS API via Syscalls */
#define rsl_ls(p) rsl_syscall(10, (uint64_t)(p), 0, 0)
#define rsl_cat(p) rsl_syscall(11, (uint64_t)(p), 0, 0)
#define rsl_write(p, c) rsl_syscall(12, (uint64_t)(p), (uint64_t)(c), 0)
#define rsl_mkdir(p) rsl_syscall(13, (uint64_t)(p), 0, 0)
#define rsl_rmdir(p) rsl_syscall(14, (uint64_t)(p), 0, 0)
#define rsl_exists(p) (bool)rsl_syscall(15, (uint64_t)(p), 0, 0)
#define rsl_mount(p) rsl_syscall(16, (uint64_t)(p), 0, 0)
#define rsl_format(p) rsl_syscall(17, (uint64_t)(p), 0, 0)
#define rsl_stamp(p) rsl_syscall(18, (uint64_t)(p), 0, 0)
#define rsl_eject(p) rsl_syscall(20, (uint64_t)(p), 0, 0)
#define rsl_scan() rsl_syscall(19, 0, 0, 0)
#define rsl_list_disks() rsl_syscall(101, 0, 0, 0)

/* VFS Handle API */
#define rsl_open(p, m) (void*)rsl_syscall(21, (uint64_t)(p), (uint64_t)(m), 0)
#define rsl_read(h, b, l) rsl_syscall(22, (uint64_t)(h), (uint64_t)(b), (uint64_t)(l))
#define rsl_write_h(h, b, l) rsl_syscall(23, (uint64_t)(h), (uint64_t)(b), (uint64_t)(l))
#define rsl_close(h) rsl_syscall(24, (uint64_t)(h), 0, 0)

/* GUI API */
#define rsl_get_fb_info(idx) rsl_syscall(202, (uint64_t)(idx), 0, 0)
#define rsl_de_start() rsl_syscall(203, 0, 0, 0)
#define rsl_get_mouse(idx) rsl_syscall(204, (uint64_t)(idx), 0, 0)
#define rsl_draw_char(c, px, py, fg, bg) rsl_syscall(213, (uint64_t)(c) | ((uint64_t)(px) << 32), (uint64_t)(py) | ((uint64_t)(fg) << 32), (uint64_t)(bg))

#else
/*
 * Kernel Mode: Direct Linkage
 */
void retain(void* ptr);
void release(void* ptr);
void* str_create(const char* cstr);
void* str_concat(void* s1, void* s2);
bool str_is_empty(void* str);
bool str_match(void* str, const char* pattern);
size_t str_len(void* str);
const char* str_to_cstr(void* str);

void set_color(color_t fg, color_t bg);
void* input(const char* prompt);
void print(const char* s);

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
void rsl_scan(void);
void rsl_eject(void* path);
#endif

void rsl_shutdown(void);
void rsl_exit(void);

#endif /* RSL_H */
