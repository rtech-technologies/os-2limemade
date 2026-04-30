#ifndef OS_STDLIB_H
#define OS_STDLIB_H

#include <stddef.h>

void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t nmemb, size_t size);

int atoi(const char* s);
void itoa(int n, char s[], int base);
long strtol(const char* nptr, char** endptr, int base);

#endif
