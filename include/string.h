#ifndef STRING_H
#define STRING_H

#include <stddef.h>

size_t strlen(const char* s);
void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
int strcmp(const char* s1, const char* s2);
char* strcpy(char* dest, const char* src);
char* strstr(const char* haystack, const char* needle);

#endif
