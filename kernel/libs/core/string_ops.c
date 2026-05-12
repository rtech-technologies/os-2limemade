#include <stddef.h>
#include <stdint.h>

void* memcpy(void* restrict dest, const void* restrict src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = s;
    for (size_t i = 0; i < n; i++) p[i] = (unsigned char)c;
    return s;
}

void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; i--) d[i - 1] = s[i - 1];
    }
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = s1;
    const unsigned char* p2 = s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return p1[i] - p2[i];
    }
    return 0;
}

#include <stdarg.h>

int vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    if (size == 0) return 0;
    size_t i = 0;
    const char* f = format;

    while (*f && i < size - 1) {
        if (*f == '%') {
            f++;
            if (*f == 's') {
                char* s = va_arg(ap, char*);
                while (*s && i < size - 1) str[i++] = *s++;
            } else if (*f == 'd') {
                int d = va_arg(ap, int);
                if (d == 0) { str[i++] = '0'; }
                else {
                    if (d < 0) { str[i++] = '-'; d = -d; }
                    char buf[16]; int bi = 0;
                    while (d > 0 && bi < 16) { buf[bi++] = (d % 10) + '0'; d /= 10; }
                    while (bi > 0 && i < size - 1) str[i++] = buf[--bi];
                }
            } else if (*f == 'x' || *f == 'p') {
                uint64_t x;
                if (*f == 'x') x = (uint64_t)va_arg(ap, uint32_t);
                else x = va_arg(ap, uint64_t);

                if (x == 0) { str[i++] = '0'; }
                else {
                    const char* hex = "0123456789ABCDEF";
                    char buf[16]; int bi = 0;
                    while (x > 0 && bi < 16) { buf[bi++] = hex[x % 16]; x /= 16; }
                    while (bi > 0 && i < size - 1) str[i++] = buf[--bi];
                }
            } else {
                str[i++] = *f;
            }
        } else {
            str[i++] = *f;
        }
        f++;
    }
    str[i] = '\0';
    return (int)i;
}

void __memset_chk(void* dest, int c, size_t n, size_t dest_len) {
    (void)dest_len;
    memset(dest, c, n);
}

void __memcpy_chk(void* dest, const void* src, size_t n, size_t dest_len) {
    (void)dest_len;
    memcpy(dest, src, n);
}
