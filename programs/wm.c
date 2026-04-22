#include <include/rsl.h>

void wm_main(void) {
    print("[RTC64] Window Manager Standalone Starting...\n");
    /* Standalone logic for WM would go here, interacting with syscalls */
    while(1) {
        /* Syscall for RTC64 Update/Draw */
        rsl_syscall(200, 0, 0, 0);
    }
}
