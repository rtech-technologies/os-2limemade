#ifndef STDLIB_H
#define STDLIB_H
#include <stddef.h>
long strtol(const char* nptr, char** endptr, int base);
void* malloc(size_t size);
void free(void* ptr);
#endif
