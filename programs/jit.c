#include <include/rsl.h>
#include <programs/libc/libc.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        print("Usage: jit <script.cl>\n");
        return 1;
    }

    print("[JIT] Loading script for execution...\n");

    /* Syscall 300: cmdlets_execute_script */
    rsl_syscall(300, (uint64_t)argv[1], 0, 0);

    print("[JIT] Execution finished.\n");
    return 0;
}
