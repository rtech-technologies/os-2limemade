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
    /* Userland Syscall: print error and hang */
    __asm__ volatile ("int $3" : : "a"((uint64_t)0), "b"((uint64_t)"ASSERTION FAILURE\n") : "memory");
    for(;;);
}
#endif
