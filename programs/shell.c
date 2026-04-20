#include <include/rsl.h>

static bool cstr_match(const char* s1, const char* s2) {
    int i = 0; while (s1[i] && s2[i]) { if (s1[i] != s2[i]) return false; i++; } return s1[i] == s2[i];
}

static bool is_absolute(const char* path) {
    int i = 0; while (path[i]) { if (path[i] == ':') return true; i++; } return path[0] == '/';
}

static void* resolve_path(void* curdir, const char* arg) {
    if (is_absolute(arg)) return str_create(arg);
    const char* cd = str_to_cstr(curdir);
    if (cstr_match(cd, "/")) return str_create(arg);
    return str_concat(curdir, str_create(arg));
}

void shell_main(void) {
    set_color(GREEN, BLACK);
    print("\n[ OSx2 Sovereign ] Core Upgrade Success.\n");
    print("1. Install to SATA HDD\n2. Enter Safe Mode\n3. Launch Window Manager (wm.bin)\n");
    void* choice = input("\nSelect Option: ");
    if (choice) {
        if (str_match(choice, "1")) { void rsl_execute_stream(const char* p); rsl_execute_stream("BOOT:/install.rsl"); }
        else if (str_match(choice, "3")) { void rsl_execute_stream(const char* p); rsl_execute_stream("BOOT:/wm.bin"); }
        release(choice);
    }

    void* curdir = str_create("/");
    while (1) {
        void* ps = str_create(" OS2>"); void* pr = str_concat(curdir, ps);
        void* cmd_line = input(str_to_cstr(pr));
        release(ps); release(pr);
        if (!cmd_line) continue;
        if (str_is_empty(cmd_line)) { release(cmd_line); continue; }

        char* line = (char*)str_to_cstr(cmd_line);
        char* argv[16]; int argc = 0; char* p = line;
        while (*p && argc < 16) {
            while (*p == ' ') *p++ = '\0'; if (*p == '\0') break;
            argv[argc++] = p; while (*p && *p != ' ') p++;
        }
        if (argc == 0) { release(cmd_line); continue; }

        if (cstr_match(argv[0], "ls")) {
            if (argc > 1) { void* path = resolve_path(curdir, argv[1]); rsl_ls(path); release(path); }
            else rsl_ls(curdir);
        } else if (cstr_match(argv[0], "cd")) {
            if (argc > 1) {
                void* np; if (cstr_match(argv[1], "/")) np = str_create("/");
                else if (cstr_match(argv[1], "..")) np = str_create("/");
                else np = resolve_path(curdir, argv[1]);
                if (cstr_match(str_to_cstr(np), "/") || rsl_exists(np)) { release(curdir); curdir = np; rsl_cd(curdir); }
                else { print("Path Fault.\n"); release(np); }
            }
        } else if (cstr_match(argv[0], "cat")) {
            if (argc > 1) { void* path = resolve_path(curdir, argv[1]); rsl_cat(path); release(path); print("\n"); }
        } else if (cstr_match(argv[0], "write")) {
            if (argc > 1 && !cstr_match(str_to_cstr(curdir), "/")) {
                void* path = resolve_path(curdir, argv[1]);
                void* content = input("Content: ");
                if (content) { rsl_write(path, content); release(content); }
                release(path);
            } else print("Denied.\n");
        } else if (cstr_match(argv[0], "mount")) {
            if (argc > 1) { void* path = str_create(argv[1]); rsl_mount(path); release(path); }
        } else if (cstr_match(argv[0], "format")) {
            if (argc > 1) { void* path = str_create(argv[1]); rsl_format(path); release(path); }
        } else if (cstr_match(argv[0], "help")) {
            print("ls, cd, cat, write, mount, format, run, wm, settings, exit\n");
        } else if (cstr_match(argv[0], "wm")) {
            void rsl_execute_stream(const char* p); rsl_execute_stream("BOOT:/wm.bin");
        } else if (cstr_match(argv[0], "settings")) rsl_settings();
        else if (cstr_match(argv[0], "exit")) { void rsl_shutdown(void); rsl_shutdown(); }
        else print("Unknown.\n");

        release(cmd_line);
    }
}
