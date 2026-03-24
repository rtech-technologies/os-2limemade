#include <include/rsl.h>

void shell_main(void) {
    color(10, 0); /* Emerald Green */
    print("Sovereign Core Online.\n");

    while (1) {
        /* Pythonic: Prints the prompt, waits, and returns the ARC-managed string */
        void* cmd_line = input("os2> ");

        /* 1. Empty Check (Kernel handles the null-terminator) */
        if (str_is_empty(cmd_line)) {
            release(cmd_line);
            continue;
        }

        /* 2. Dispatcher (No character-by-character checks) */
        if (str_match(cmd_line, "ls")) {
            rsl_ls();
        }
        else if (str_match(cmd_line, "write")) {
            /* Interactive write: Simple and readable */
            void* path = input("Enter Filename: ");
            void* content = input("Enter Content: ");

            rsl_write(path, content);

            release(path);
            release(content);
        }
        else if (str_match(cmd_line, "exit")) {
            release(cmd_line);
            break; /* Clean exit to main.c for ACPI shutdown */
        }
        else {
            print("Unknown Command.\n");
        }

        /* Always release the command line before the next prompt */
        release(cmd_line);
        break; /* Exit for build verification purposes */
    }
}
