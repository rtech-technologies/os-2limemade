#include <include/rsl.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        print("Usage: jit <script.cl>\n");
        return 1;
    }

    print("[JIT] Starting compiler-engine...\n");

    /* Standalone programs use syscall ID 300 for script execution */
    rsl_syscall(300, (uint64_t)argv[1], 0, 0);

    print("[JIT] Execution finished.\n");
    return 0;
}
