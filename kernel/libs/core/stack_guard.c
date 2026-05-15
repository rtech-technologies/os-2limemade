#include <stdint.h>
#include <stddef.h>

uint64_t __stack_chk_guard = 0xDEADC0DEBEEFCAFEULL;

void __stack_chk_fail(void) {
    extern void quartermaster_panic(const char* msg, void* state);
    quartermaster_panic("STACK SMASH DETECTED", NULL);
}

void stack_guard_init(void) {
    /* Sovereign Fix: Initialize stack guard with hardware entropy if available */
    uint64_t seed;
    __asm__ volatile ("rdrand %0" : "=r"(seed));
    if (seed) __stack_chk_guard = seed;
}
