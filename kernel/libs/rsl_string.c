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
