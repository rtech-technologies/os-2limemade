#include <include/rsl.h>
#include <include/vfs.h>
#include <stdbool.h>
#include <stdint.h>

void* str_create(const char* cstr);
void vga_clear(void);

static int split_argv(char* line, char** argv, int max_argv) {
    int argc = 0;
    char* p = line;
    while (*p && argc < max_argv) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) {
            *p = '\0';
            p++;
        }
    }
    return argc;
}

void rsl_dispatch_command(char* line, void** curdir_ptr, bool* is_safe_ptr) {
    (void)is_safe_ptr;
    char* argv[16];
    int argc = split_argv(line, argv, 16);
    if (argc == 0) return;

    if (str_match(argv[0], "help")) {
        print("Sovereign OS Shell Commands:\n");
        print("  ls [path]    - List directory\n");
        print("  cd <path>    - Change directory\n");
        print("  cat <file>   - Display file content\n");
        print("  clear        - Clear screen\n");
        print("  echo [text]  - Print text\n");
        print("  help         - Show this help\n");
        print("  exit         - Return to login\n");
    } else if (str_match(argv[0], "ls")) {
        void* path = (argc > 1) ? str_create(argv[1]) : str_create(str_to_cstr(*curdir_ptr));
        vfs_ls(path);
        release(path);
    } else if (str_match(argv[0], "cd")) {
        if (argc > 1) {
            void* new_dir = str_create(argv[1]);
            if (vfs_exists(new_dir)) {
                release(*curdir_ptr);
                *curdir_ptr = new_dir;
            } else {
                print("Error: Path not found.\n");
                release(new_dir);
            }
        }
    } else if (str_match(argv[0], "cat")) {
        if (argc > 1) {
            void* path = str_create(argv[1]);
            vfs_cat(path);
            release(path);
        }
    } else if (str_match(argv[0], "clear")) {
        vga_clear();
    } else if (str_match(argv[0], "echo")) {
        for (int i = 1; i < argc; i++) {
            print(argv[i]);
            print(" ");
        }
        print("\n");
    } else if (str_match(argv[0], "exit")) {
        /* Reset and return to login - handled by shell.c loop */
        print("Exiting Sovereign Session...\n");
    } else {
        print("Unknown command: ");
        print(argv[0]);
        print("\nType 'help' for assistance.\n");
    }
}
