#include <include/rsl.h>
#include <stdint.h>

void syscall_fuzz_harness(void) {
    uint8_t dummy_buffer[1024];
    for (int i = 0; i < 1000; i++) {
        /* Pseudo-random junk calls to test robust parameter validation */
        void* p = (void*)(uint64_t)(i * 0x1234567);
        rsl_ls(p);
        rsl_cat(p);
        rsl_write(p, (void*)dummy_buffer);
    }
}
