#ifndef RSL_H
#define RSL_H

#include <stdint.h>
#include <stddef.h>

/* RSL Types */
typedef void* managed_ptr_t;

/* ARC Memory Management */
void retain(managed_ptr_t ptr);
void release(managed_ptr_t ptr);

/* String API */
managed_ptr_t str_create(const char* cstr);
size_t str_len(managed_ptr_t str);
const char* str_to_cstr(managed_ptr_t str);

/* Console API */
void color(uint8_t fg, uint8_t bg);
managed_ptr_t readline(void);
void print(managed_ptr_t str);
void print_cstr(const char* cstr);

/* File System API (RSL Wrappers) */
typedef struct {
    uint32_t handle;
} RSL_FILE;

RSL_FILE* rsl_f_open(managed_ptr_t path, const char* mode);
void rsl_f_close(RSL_FILE* file);
size_t rsl_f_read(RSL_FILE* file, void* buffer, size_t count);
void rsl_f_write(RSL_FILE* file, const void* buffer, size_t count);

#endif /* RSL_H */
