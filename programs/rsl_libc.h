#ifndef RSL_LIBC_H
#define RSL_LIBC_H

#include <stdint.h>
#include <stddef.h>

/* Sovereign ISA RSL Runtime: Minimal C Library Implementation */

static inline void* rsl_memcpy(void* dst, const void* src, size_t n) {
    char* d = (char*)dst;
    const char* s = (const char*)src;
    while(n--) *d++ = *s++;
    return dst;
}

static inline void* rsl_memset(void* dst, int v, size_t n) {
    char* d = (char*)dst;
    while(n--) *d++ = (char)v;
    return dst;
}

static inline size_t rsl_strlen(const char* s) {
    size_t l = 0;
    while(*s++) l++;
    return l;
}

static inline int rsl_memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = s1;
    const unsigned char* p2 = s2;
    while(n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++; p2++;
    }
    return 0;
}

static inline void rsl_qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*)) {
    if (nmemb < 2) return;
    uint8_t* array = (uint8_t*)base;
    for (size_t i = 1; i < nmemb; i++) {
        uint8_t tmp[256]; /* Sufficient for Nuklear/STB types */
        rsl_memcpy(tmp, array + i * size, size);
        int j = (int)i - 1;
        while (j >= 0 && compar(array + j * size, tmp) > 0) {
            rsl_memcpy(array + (j + 1) * size, array + j * size, size);
            j--;
        }
        rsl_memcpy(array + (j + 1) * size, tmp, size);
    }
}

/* Minimal Math for STB_TrueType / Nuklear */
static inline double rsl_fabs(double x) { return x < 0 ? -x : x; }
static inline double rsl_floor(double x) {
    int i = (int)x;
    if (x < 0 && x != (double)i) return (double)(i - 1);
    return (double)i;
}
static inline double rsl_ceil(double x) {
    int i = (int)x;
    if (x > 0 && x != (double)i) return (double)(i + 1);
    return (double)i;
}
static inline double rsl_sqrt(double x) {
    if (x <= 0) return 0;
    double res = x;
    for (int i = 0; i < 10; i++) res = 0.5 * (res + x / res);
    return res;
}
static inline double rsl_fmod(double x, double y) {
    if (y == 0) return 0;
    return x - (int)(x / y) * y;
}
static inline double rsl_pow(double x, double y) {
    (void)y; /* STBTT uses this for small integers usually */
    if (y == 0) return 1.0;
    double res = x;
    for (int i = 1; i < (int)y; i++) res *= x;
    return res;
}
static inline double rsl_sin(double x) {
    /* Taylor expansion: x - x^3/6 + x^5/120 - x^7/5040 */
    double x2 = x * x;
    double x3 = x2 * x;
    double x5 = x3 * x2;
    double x7 = x5 * x2;
    return x - (x3 / 6.0) + (x5 / 120.0) - (x7 / 5040.0);
}
static inline double rsl_cos(double x) {
    /* Taylor expansion: 1 - x^2/2 + x^4/24 - x^6/720 */
    double x2 = x * x;
    double x4 = x2 * x2;
    double x6 = x4 * x2;
    return 1.0 - (x2 / 2.0) + (x4 / 24.0) - (x6 / 720.0);
}
static inline double rsl_acos(double x) {
    /* Extremely crude approximation: pi/2 - x */
    return 1.570796 - x;
}

#endif
