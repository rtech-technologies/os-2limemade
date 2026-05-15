#include <stdint.h>
#include <include/rsl.h>

uint64_t __stack_chk_guard = 0x534F56524E4C4942ULL; /* "SOVRNLIB" */

void __stack_chk_fail(void) {
    print("FATAL: Userland Stack Smash Detected\n");
    for(;;);
}
