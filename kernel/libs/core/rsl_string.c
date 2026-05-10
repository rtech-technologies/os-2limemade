#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void* arc_alloc(size_t size);

typedef struct {
    size_t length;
    char data[];
} rsl_string_t;

void* str_create(const char* cstr) {
    if (!cstr) return NULL;

    size_t len = 0;
    while (cstr[len]) len++;

    rsl_string_t* r_str = (rsl_string_t*)arc_alloc(sizeof(rsl_string_t) + len + 1);
    if (!r_str) return NULL;

    r_str->length = len;
    for (size_t i = 0; i < len; i++) {
        r_str->data[i] = cstr[i];
    }
    r_str->data[len] = '\0';

    return (void*)r_str;
}

static void* clipboard = NULL;

void rsl_copy(void* str) {
    if (clipboard) release(clipboard);
    if (str) {
        retain(str);
        clipboard = str;
    } else {
        clipboard = NULL;
    }
}

void* rsl_paste(void) {
    if (clipboard) {
        retain(clipboard);
        return clipboard;
    }
    return NULL;
}

bool str_is_empty(void* str) {
    if (!str) return true;
    return ((rsl_string_t*)str)->length == 0;
}

bool str_match(void* str, const char* pattern) {
    if (!str || !pattern) return false;
    const char* s = ((rsl_string_t*)str)->data;
    size_t i = 0;
    while (s[i] && pattern[i]) {
        if (s[i] != pattern[i]) return false;
        i++;
    }
    return (s[i] == pattern[i]);
}

size_t str_len(void* str) {
    if (!str) return 0;
    return ((rsl_string_t*)str)->length;
}

const char* str_to_cstr(void* str) {
    if (!str) return "";
    return ((rsl_string_t*)str)->data;
}

void* str_concat(void* s1, void* s2) {
    if (!s1) { retain(s2); return s2; }
    if (!s2) { retain(s1); return s1; }

    size_t len1 = str_len(s1);
    size_t len2 = str_len(s2);

    rsl_string_t* r_str = (rsl_string_t*)arc_alloc(sizeof(rsl_string_t) + len1 + len2 + 1);
    if (!r_str) return NULL;

    r_str->length = len1 + len2;
    const char* c1 = str_to_cstr(s1);
    const char* c2 = str_to_cstr(s2);

    for (size_t i = 0; i < len1; i++) r_str->data[i] = c1[i];
    for (size_t i = 0; i < len2; i++) r_str->data[len1 + i] = c2[i];
    r_str->data[len1 + len2] = '\0';

    return (void*)r_str;
}

void* memset(void* s, int c, size_t n) {
    uint8_t* p = (uint8_t*)s;
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)c;
    return s;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
    return *(uint8_t*)s1 - *(uint8_t*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++; s2++; n--;
    }
    if (n == 0) return 0;
    return *(uint8_t*)s1 - *(uint8_t*)s2;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return NULL;
}

size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    (void)format;
    if (size > 0) str[0] = '\0';
    return 0;
}

long int strtol(const char *nptr, char **endptr, int base) {
    long int res = 0;
    int k = 0;
    while (nptr[k] >= '0' && nptr[k] <= '9') {
        res = res * base + (nptr[k] - '0');
        k++;
    }
    if (endptr) *endptr = (char*)&nptr[k];
    return res;
}
