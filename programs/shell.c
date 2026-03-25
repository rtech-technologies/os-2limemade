#include <include/rsl.h>

static bool cstr_match(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] && s2[i]) {
        if (s1[i] != s2[i]) return false;
        i++;
    }
    return s1[i] == s2[i];
}

static bool cstr_starts_with(const char* str, const char* prefix) {
    int i = 0;
    while (prefix[i]) {
        if (str[i] != prefix[i]) return false;
        i++;
    }
    return true;
}

static color_t name_to_color(const char* name) {
    if (cstr_match(name, "black")) return BLACK;
    if (cstr_match(name, "blue")) return BLUE;
    if (cstr_match(name, "green")) return GREEN;
    if (cstr_match(name, "cyan")) return CYAN;
    if (cstr_match(name, "red")) return RED;
    if (cstr_match(name, "magenta")) return MAGENTA;
    if (cstr_match(name, "brown")) return BROWN;
    if (cstr_match(name, "white")) return WHITE;
    return WHITE;
}

void shell_main(void) {
    set_color(GREEN, BLACK);
    print("OSx2 Limemade: OS boot sucsess\n");

    void* curdir = str_create("/");

    while (1) {
        void* suffix = str_create("OS2>");
        void* prompt = str_concat(curdir, suffix);
        release(suffix);

        void* cmd_line = input(str_to_cstr(prompt));
        release(prompt);

        if (cmd_line == NULL) {
            continue; /* Break happened (Escape key) */
        }

        if (str_is_empty(cmd_line)) {
            release(cmd_line);
            continue;
        }

        const char* cmd = str_to_cstr(cmd_line);

        if (cstr_match(cmd, "ls")) {
            rsl_ls(curdir);
        }
        else if (cstr_starts_with(cmd, "cat ")) {
            void* path = str_create(cmd + 4);
            rsl_cat(path);
            release(path);
        }
        else if (cstr_match(cmd, "write")) {
            void* filename = input("Enter Filename: ");
            if (!filename) continue;

            void* content = input("Enter Content: ");
            if (!content) { release(filename); continue; }

            rsl_write(filename, content);
            release(filename);
            release(content);
        }
        else if (cstr_starts_with(cmd, "cd ")) {
            release(curdir);
            curdir = str_create(cmd + 3);
            rsl_cd(curdir);
        }
        else if (cstr_match(cmd, "color")) {
            void* fg_name = input("Enter FG Color: ");
            if (!fg_name) continue;
            void* bg_name = input("Enter BG Color: ");
            if (!bg_name) { release(fg_name); continue; }

            set_color(name_to_color(str_to_cstr(fg_name)), name_to_color(str_to_cstr(bg_name)));
            release(fg_name);
            release(bg_name);
            print("Color Updated.\n");
        }
        else if (cstr_match(cmd, "help")) {
            print("Available Commands:\n");
            print("ls          - List disks/partitions/files\n");
            print("cd <path>   - Change directory context (e.g. 0:/0/)\n");
            print("cat <file>  - Read file content\n");
            print("write       - Interactive file creation\n");
            print("color       - Interactive color change\n");
            print("help        - Show this menu\n");
            print("exit        - Terminate shell\n");
        }
        else if (cstr_match(cmd, "exit")) {
            release(cmd_line);
            break;
        }
        else {
            print("Unknown Command. Type 'help' for options.\n");
        }

        release(cmd_line);
    }

    release(curdir);
}
