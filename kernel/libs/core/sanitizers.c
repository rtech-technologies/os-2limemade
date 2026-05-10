#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* SSE Stubs for Kernel context */
void _sse_init(void) {
    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1 << 2); /* Clear EM bit */
    cr0 |= (1 << 1);  /* Set MP bit */
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));

    uint64_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (3 << 9); /* Set OSFXSR and OSXMMEXCPT bits */
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4));
}

#ifndef USERLAND_SANITIZER
void __assert_fail(const char * assertion, const char * file, unsigned int line, const char * function) {
    (void)assertion; (void)file; (void)line; (void)function;
    extern void quartermaster_panic(const char* msg, void* state);
    quartermaster_panic("ASSERTION FAILURE", NULL);
}
#else
void __assert_fail(const char * assertion, const char * file, unsigned int line, const char * function) {
    (void)assertion; (void)file; (void)line; (void)function;
    __asm__ volatile ("int $3" : : "a"((uint64_t)0), "b"((uint64_t)"ASSERTION FAILURE\n") : "memory");
    for(;;);
}
#endif

/* Undefined Behavior Sanitizer Handlers */
void __ubsan_handle_add_overflow(void* data, void* lhs, void* rhs) { (void)data; (void)lhs; (void)rhs; }
void __ubsan_handle_sub_overflow(void* data, void* lhs, void* rhs) { (void)data; (void)lhs; (void)rhs; }
void __ubsan_handle_mul_overflow(void* data, void* lhs, void* rhs) { (void)data; (void)lhs; (void)rhs; }
void __ubsan_handle_divrem_overflow(void* data, void* lhs, void* rhs) { (void)data; (void)lhs; (void)rhs; }
void __ubsan_handle_negate_overflow(void* data, void* val) { (void)data; (void)val; }
void __ubsan_handle_pointer_overflow(void* data, void* lhs, void* rhs) { (void)data; (void)lhs; (void)rhs; }
void __ubsan_handle_shift_out_of_bounds(void* data, void* lhs, void* rhs) { (void)data; (void)lhs; (void)rhs; }
void __ubsan_handle_out_of_bounds(void* data, void* idx) { (void)data; (void)idx; }
void __ubsan_handle_type_mismatch_v1(void* data, void* ptr) { (void)data; (void)ptr; }
void __ubsan_handle_vla_bound_not_positive(void* data, void* bound) { (void)data; (void)bound; }
void __ubsan_handle_load_invalid_value(void* data, void* val) { (void)data; (void)val; }
#ifndef USERLAND_SANITIZER
void __ubsan_handle_builtin_unreachable(void* data) {
    (void)data;
    extern void quartermaster_panic(const char* msg, void* state);
    quartermaster_panic("UBSAN: REACHED UNREACHABLE", NULL);
}
#else
void __ubsan_handle_builtin_unreachable(void* data) {
    (void)data;
    __asm__ volatile ("int $3" : : "a"((uint64_t)0), "b"((uint64_t)"UBSAN: REACHED UNREACHABLE\n") : "memory");
    for(;;);
}
#endif
void __ubsan_handle_nonnull_arg(void* data) { (void)data; }
void __ubsan_handle_type_mismatch(void* data, void* ptr) { (void)data; (void)ptr; }
void __ubsan_handle_nonnull_return(void* data) { (void)data; }
void __ubsan_handle_vla_bound_not_positive_v1(void* data, void* bound) { (void)data; (void)bound; }
