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
