#ifndef OS_STDLIB_H
#define OS_STDLIB_H

#include <stddef.h>

void* malloc(size_t size);
void free(void* ptr);
int atoi(const char* s);

#endif
void itoa(int n, char s[], int base);
