#include <include/rsl.h>
#include <kernel/unice64/task.h>
#include <stddef.h>

void rsl_dispatch_command(char* line, void** curdir_ptr, bool* is_safe_ptr);

void kernel_fallback_shell(void) {
    set_color(YELLOW, BLACK);
    print("\n[ OSx2 KERNEL FALLBACK SHELL ]\n");
    print("Standalone shell.bin failed to load. Recovery mode active.\n\n");

    bool is_safe = true; /* Default to safe mode for fallback */
    void* curdir = str_create("/");

    while (1) {
        void* suffix = str_create(" (FALLBACK)>");
        void* prompt = str_concat(curdir, suffix);
        release(suffix);

        void* cmd_line = input(str_to_cstr(prompt));
        release(prompt);

        if (cmd_line == NULL) continue;
        if (str_is_empty(cmd_line)) { release(cmd_line); continue; }

        /* rsl_dispatch_command modifies the line in-place for argv splitting */
        rsl_dispatch_command((char*)str_to_cstr(cmd_line), &curdir, &is_safe);
        release(cmd_line);
    }
}
