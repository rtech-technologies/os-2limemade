#include <stdint.h>
#include <stddef.h>

#ifndef USERLAND_SANITIZER
void quartermaster_panic(const char* msg, void* state);
#define SANITIZER_PANIC(msg) quartermaster_panic(msg, NULL)
#else
#include <include/rsl.h>
#define SANITIZER_PANIC(msg) { print(msg); print("\n"); for(;;); }
#endif

struct ubsan_source_location {
    const char *file;
    uint32_t line;
    uint32_t column;
};

struct ubsan_type_descriptor {
    uint16_t kind;
    uint16_t info;
    char name[];
};

struct ubsan_type_mismatch_data {
    struct ubsan_source_location location;
    struct ubsan_type_descriptor *type;
    uintptr_t alignment;
    uint8_t type_check_kind;
};

void __ubsan_handle_type_mismatch_v1(struct ubsan_type_mismatch_data *data, uintptr_t ptr) {
    if (!ptr) {
        SANITIZER_PANIC("UBSAN: NULL POINTER ACCESS");
    } else if (ptr & (data->alignment - 1)) {
        SANITIZER_PANIC("UBSAN: UNALIGNED ACCESS");
    } else {
        SANITIZER_PANIC("UBSAN: TYPE MISMATCH");
    }
}

void __ubsan_handle_pointer_overflow(void* data, uintptr_t base, uintptr_t result) {
    (void)data; (void)base; (void)result;
    SANITIZER_PANIC("UBSAN: POINTER OVERFLOW");
}

void __ubsan_handle_out_of_bounds(void* data, uintptr_t index) {
    (void)data; (void)index;
    SANITIZER_PANIC("UBSAN: ARRAY OUT OF BOUNDS");
}

void __ubsan_handle_shift_out_of_bounds(void* data, uintptr_t lhs, uintptr_t rhs) {
    (void)data; (void)lhs; (void)rhs;
    SANITIZER_PANIC("UBSAN: SHIFT OUT OF BOUNDS");
}

void __ubsan_handle_load_invalid_value(void* data, uintptr_t val) {
    (void)data; (void)val;
    SANITIZER_PANIC("UBSAN: LOAD INVALID VALUE");
}

void __ubsan_handle_divrem_overflow(void* data, void* lhs, void* rhs) {
    (void)data; (void)lhs; (void)rhs;
    SANITIZER_PANIC("UBSAN: DIVREM OVERFLOW");
}

void __ubsan_handle_add_overflow(void* data, void* lhs, void* rhs) {
    (void)data; (void)lhs; (void)rhs;
    SANITIZER_PANIC("UBSAN: ADD OVERFLOW");
}

void __ubsan_handle_sub_overflow(void* data, void* lhs, void* rhs) {
    (void)data; (void)lhs; (void)rhs;
    SANITIZER_PANIC("UBSAN: SUB OVERFLOW");
}

void __ubsan_handle_mul_overflow(void* data, void* lhs, void* rhs) {
    (void)data; (void)lhs; (void)rhs;
    SANITIZER_PANIC("UBSAN: MUL OVERFLOW");
}

void __ubsan_handle_negate_overflow(void* data, void* val) {
    (void)data; (void)val;
    SANITIZER_PANIC("UBSAN: NEGATE OVERFLOW");
}

void __ubsan_handle_vla_bound_not_positive(void* data, void* bound) {
    (void)data; (void)bound;
    SANITIZER_PANIC("UBSAN: VLA BOUND NOT POSITIVE");
}

void __ubsan_handle_nonnull_arg(void* data) {
    (void)data;
    SANITIZER_PANIC("UBSAN: NONNULL ARG");
}

void __ubsan_handle_nonnull_return(void* data) {
    (void)data;
    SANITIZER_PANIC("UBSAN: NONNULL RETURN");
}

void __ubsan_handle_builtin_unreachable(void* data) {
    (void)data;
    SANITIZER_PANIC("UBSAN: REACHED UNREACHABLE");
}
