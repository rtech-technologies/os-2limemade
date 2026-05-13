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

typedef void* vfs_handle_user_t;
vfs_handle_user_t rsl_open(void* path, const char* mode);
int rsl_read(vfs_handle_user_t handle, void* buf, int len);
void rsl_close(vfs_handle_user_t handle);

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
void rsl_update(void);
void rsl_upgrade(void);

/* Storage and Partitioning Syscalls */
int rsl_list_disks(void);
bool rsl_is_sovereign(int disk_id);
int rsl_get_disk_info(int disk_id, char* name, uint64_t* size);
int rsl_partition_disk(int id);
int rsl_format_disk(int id);

void rsl_user_create(const char* name, const char* pass, int* out_res);
void rsl_mount_vfs(int disk_id, const char* name, int* out_res);

void rsl_dispatch_command(char* line, void** curdir_ptr, bool* is_safe_ptr);

/* Graphics Syscall */
typedef struct {
    uint64_t address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
} rsl_fb_t;
int rsl_get_fb(rsl_fb_t* fb);
void rsl_input(const char* prompt, char* buffer);
void sys_yield(void);
void rsl_yield(void);
char* strstr(const char* haystack, const char* needle);

/* GUI Helpers */
void gui_draw_rect(rsl_fb_t* fb, int x, int y, int w, int h, uint32_t color);
void gui_draw_text(rsl_fb_t* fb, int x, int y, const char* text, uint32_t color);

/* RTECH Binary Header */
typedef struct {
    char magic[8];      /* "RTECH01\0" */
    uint64_t entry_offset;
    uint64_t reserved[2];
} rtech_header_t;

#endif /* RSL_H */
