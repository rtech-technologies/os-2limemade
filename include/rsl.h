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
managed_ptr_t input(managed_ptr_t prompt);
void print(managed_ptr_t str);
void print_cstr(const char* cstr);

/* File System API (RSL Wrappers) */
void rsl_ls(managed_ptr_t path);
void rsl_cat(managed_ptr_t path);
void rsl_write(managed_ptr_t path, managed_ptr_t content);
void rsl_cd(managed_ptr_t path);

#endif /* RSL_H */
