#ifndef LIBC_H
#define LIBC_H

#include <stddef.h>
#include <stdint.h>

void* malloc(size_t size);
void free(void* ptr);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);

void* memcpy(void* dst, const void* src, size_t n);
void* memset(void* dst, int v, size_t n);
size_t strlen(const char* s);
int memcmp(const void* s1, const void* s2, size_t n);
char* strcpy(char* dst, const char* src);
int strcmp(const char* s1, const char* s2);

double fabs(double x);
double floor(double x);
double ceil(double x);
double sqrt(double x);
double pow(double x, double y);
double sin(double x);
double cos(double x);

#endif
