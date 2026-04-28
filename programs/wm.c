#include <include/rsl.h>

int main(void) {
    print("[RTC64] Window Manager Standalone Starting...\n");
    while(1) {
        /* Syscall for RTC64 Update/Draw */
        rsl_syscall(200, 0, 0, 0);
    }
    return 0;
}
