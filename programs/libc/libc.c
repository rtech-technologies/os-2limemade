#include <stdint.h>
#include <stddef.h>

#ifdef RSL_BINARY_MODE
/* Userland Syscall Wrapper */
static inline uint64_t rsl_syscall(uint64_t id, uint64_t a1, uint64_t a2, uint64_t a3) {
    uint64_t ret;
    __asm__ volatile ("int $0x03" : "=a"(ret) : "a"(id), "b"(a1), "c"(a2), "d"(a3) : "memory");
    return ret;
}

/* Simple Heap for Userland */
static uint8_t* heap_base = NULL;
static size_t heap_offset = 0;
#define HEAP_SIZE (4 * 1024 * 1024)

typedef struct {
    size_t size;
} alloc_header_t;

void* malloc(size_t size) {
    if (!heap_base) {
        heap_base = (uint8_t*)rsl_syscall(201, 0, 0, 0); /* Get Slab Base */
        if (!heap_base) return NULL;
        heap_offset = 1024; /* Reserved for metadata if needed */
    }

    size_t total = size + sizeof(alloc_header_t);
    total = (total + 15) & ~15;
    if (heap_offset + total > HEAP_SIZE) return NULL;

    alloc_header_t* hdr = (alloc_header_t*)(heap_base + heap_offset);
    hdr->size = size;
    void* ptr = (void*)(hdr + 1);
    heap_offset += total;
    return ptr;
}

void free(void* ptr) { (void)ptr; }

void* calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void* ptr = malloc(total);
    if (!ptr) return NULL;
    for (size_t i = 0; i < total; i++) ((uint8_t*)ptr)[i] = 0;
    return ptr;
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    alloc_header_t* hdr = (alloc_header_t*)ptr - 1;
    size_t old_size = hdr->size;
    void* new_ptr = malloc(size);
    if (!new_ptr) return NULL;
    size_t copy_size = (size < old_size) ? size : old_size;
    for (size_t i = 0; i < copy_size; i++) ((uint8_t*)new_ptr)[i] = ((uint8_t*)ptr)[i];
    return new_ptr;
}

#endif

/* String functions */
size_t strlen(const char* s) {
    size_t l = 0;
    while(*s++) l++;
    return l;
}

void* memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = dst;
    const uint8_t* s = src;
    while(n--) *d++ = *s++;
    return dst;
}

void* memset(void* dst, int v, size_t n) {
    uint8_t* d = dst;
    while(n--) *d++ = (uint8_t)v;
    return dst;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* p1 = s1;
    const uint8_t* p2 = s2;
    while(n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

char* strcpy(char* dst, const char* src) {
    char* d = dst;
    while((*d++ = *src++));
    return dst;
}

int strcmp(const char* s1, const char* s2) {
    while(*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

/* Math */
double fabs(double x) { return x < 0 ? -x : x; }
double floor(double x) {
    int i = (int)x;
    return (x < 0 && x != (double)i) ? (double)(i - 1) : (double)i;
}
double ceil(double x) {
    int i = (int)x;
    return (x > 0 && x != (double)i) ? (double)(i + 1) : (double)i;
}
double sqrt(double x) {
    if (x <= 0) return 0;
    double res = x;
    for (int i = 0; i < 10; i++) res = 0.5 * (res + x / res);
    return res;
}
double pow(double x, double y) {
    if (y == 0) return 1.0;
    double res = x;
    for (int i = 1; i < (int)y; i++) res *= x;
    return res;
}
double sin(double x) {
    double x2 = x * x;
    double x3 = x2 * x;
    double x5 = x3 * x2;
    double x7 = x5 * x2;
    return x - (x3 / 6.0) + (x5 / 120.0) - (x7 / 5040.0);
}
double cos(double x) {
    double x2 = x * x;
    double x4 = x2 * x2;
    double x6 = x4 * x2;
    return 1.0 - (x2 / 2.0) + (x4 / 24.0) - (x6 / 720.0);
}
