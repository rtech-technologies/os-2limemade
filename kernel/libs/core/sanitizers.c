#include <stdint.h>
#include <stddef.h>

void forensic_panic(const char* message, void* state);

struct source_location {
    const char *file_name;
    uint32_t line;
    uint32_t column;
};

struct type_descriptor {
    uint16_t type_kind;
    uint16_t type_info;
    char type_name[1];
};

struct type_mismatch_data {
    struct source_location location;
    struct type_descriptor *type;
    unsigned long alignment;
    uint8_t type_check_kind;
};

void __ubsan_handle_type_mismatch_v1(struct type_mismatch_data *data, uintptr_t ptr) {
    (void)data; (void)ptr;
    forensic_panic("UBSAN: TYPE MISMATCH", NULL);
}

void __ubsan_handle_add_overflow(void* data, void* lhs, void* rhs) {
    (void)data; (void)lhs; (void)rhs;
    forensic_panic("UBSAN: ADD OVERFLOW", NULL);
}

void __ubsan_handle_sub_overflow(void* data, void* lhs, void* rhs) {
    (void)data; (void)lhs; (void)rhs;
    forensic_panic("UBSAN: SUB OVERFLOW", NULL);
}

void __ubsan_handle_mul_overflow(void* data, void* lhs, void* rhs) {
    (void)data; (void)lhs; (void)rhs;
    forensic_panic("UBSAN: MUL OVERFLOW", NULL);
}

void __ubsan_handle_out_of_bounds(void* data, void* idx) {
    (void)data; (void)idx;
    forensic_panic("UBSAN: OUT OF BOUNDS", NULL);
}

/* KASAN Stubs */
void __asan_load1(uintptr_t addr) { (void)addr; }
void __asan_load2(uintptr_t addr) { (void)addr; }
void __asan_load4(uintptr_t addr) { (void)addr; }
void __asan_load8(uintptr_t addr) { (void)addr; }
void __asan_store1(uintptr_t addr) { (void)addr; }
void __asan_store2(uintptr_t addr) { (void)addr; }
void __asan_store4(uintptr_t addr) { (void)addr; }
void __asan_store8(uintptr_t addr) { (void)addr; }
void __asan_handle_no_return(void) { }

void __ubsan_handle_add_overflow_abort(void* data, void* lhs, void* rhs) {
    (void)data; (void)lhs; (void)rhs;
    forensic_panic("UBSAN: ADD OVERFLOW ABORT", NULL);
}

void __ubsan_handle_type_mismatch_v1_abort(void* data, uintptr_t ptr) { (void)data; (void)ptr; forensic_panic("UBSAN: TYPE MISMATCH ABORT", NULL); }
void __ubsan_handle_pointer_overflow_abort(void* data, uintptr_t base, uintptr_t result) { (void)data; (void)base; (void)result; forensic_panic("UBSAN: POINTER OVERFLOW ABORT", NULL); }
void __ubsan_handle_sub_overflow_abort(void* data, void* lhs, void* rhs) { (void)data; (void)lhs; (void)rhs; forensic_panic("UBSAN: SUB OVERFLOW ABORT", NULL); }
void __ubsan_handle_mul_overflow_abort(void* data, void* lhs, void* rhs) { (void)data; (void)lhs; (void)rhs; forensic_panic("UBSAN: MUL OVERFLOW ABORT", NULL); }
void __ubsan_handle_divrem_overflow_abort(void* data, void* lhs, void* rhs) { (void)data; (void)lhs; (void)rhs; forensic_panic("UBSAN: DIVREM OVERFLOW ABORT", NULL); }
void __ubsan_handle_out_of_bounds_abort(void* data, void* idx) { (void)data; (void)idx; forensic_panic("UBSAN: OUT OF BOUNDS ABORT", NULL); }
void __ubsan_handle_load_invalid_value_abort(void* data, void* val) { (void)data; (void)val; forensic_panic("UBSAN: INVALID LOAD ABORT", NULL); }

void __ubsan_handle_shift_out_of_bounds_abort(void* data, void* lhs, void* rhs) {
    (void)data; (void)lhs; (void)rhs;
    forensic_panic("UBSAN: SHIFT OUT OF BOUNDS ABORT", NULL);
}

void __ubsan_handle_nonnull_arg_abort(void* data) { (void)data; forensic_panic("UBSAN: NONNULL ARG ABORT", NULL); }

void __ubsan_handle_vla_bound_not_positive_abort(void* data, uintptr_t bound) { (void)data; (void)bound; forensic_panic("UBSAN: VLA BOUND ABORT", NULL); }
void __ubsan_handle_negate_overflow_abort(void* data, void* val) { (void)data; (void)val; forensic_panic("UBSAN: NEGATE OVERFLOW ABORT", NULL); }
