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
void color(uint8_t fg, uint8_t bg);
void* input(const char* prompt);
void print(const char* s);

/* File System API (RSL Wrappers) */
void rsl_ls(void* path);
void rsl_cat(void* path);
void rsl_write(void* path, void* content);
void rsl_cd(void* path);

#endif /* RSL_H */
