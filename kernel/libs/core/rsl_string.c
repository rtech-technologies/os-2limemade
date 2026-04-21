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
    void* memcpy(void* dest, const void* src, size_t n);
    memcpy(r_str->data, cstr, len);
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

    void* memcpy(void* dest, const void* src, size_t n);
    memcpy(r_str->data, c1, len1);
    memcpy(r_str->data + len1, c2, len2);
    r_str->data[len1 + len2] = '\0';

    return (void*)r_str;
}
