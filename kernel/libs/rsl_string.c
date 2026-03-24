#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

void* arc_alloc(size_t size);

typedef struct {
    size_t length;
    char data[];
} rsl_string_t;

managed_ptr_t str_create(const char* cstr) {
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

    return (managed_ptr_t)r_str;
}

size_t str_len(managed_ptr_t str) {
    if (!str) return 0;
    return ((rsl_string_t*)str)->length;
}

const char* str_to_cstr(managed_ptr_t str) {
    if (!str) return "";
    return ((rsl_string_t*)str)->data;
}

managed_ptr_t str_concat(managed_ptr_t s1, managed_ptr_t s2) {
    if (!s1) return retain(s2), s2;
    if (!s2) return retain(s1), s1;

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

    return (managed_ptr_t)r_str;
}
