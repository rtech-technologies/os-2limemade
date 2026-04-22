#include <include/rsl.h>

void text_editor_main(void) {
    print("[OSx2] Text Editor starting...\n");
    while(1) {
        /* Syscall for Editor loop */
        rsl_syscall(201, 0, 0, 0);
    }
}
