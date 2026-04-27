#include <include/rsl.h>

int main(void) {
    print("[OSx2] Text Editor starting...\n");
    while(1) {
        /* Syscall for Editor loop */
        rsl_syscall(201, 0, 0, 0);
    }
    return 0;
}
