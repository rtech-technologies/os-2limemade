#include <stdint.h>
void spin_lock(volatile uint32_t* lock) {
    while (__atomic_test_and_set(lock, __ATOMIC_ACQUIRE)) { __asm__ volatile("pause"); }
}
void spin_unlock(volatile uint32_t* lock) {
    __atomic_clear(lock, __ATOMIC_RELEASE);
}
