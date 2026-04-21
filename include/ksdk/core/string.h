#ifndef KSDK_STRING_H
#define KSDK_STRING_H
#include <ksdk/core/types.h>
#include <include/rsl.h>
void* str_create(const char* cstr);
void* str_concat(void* s1, void* s2);
bool str_match(void* str, const char* pattern);
bool str_is_empty(void* str);
size_t str_len(void* str);
const char* str_to_cstr(void* str);
#endif
