#include <include/rsl.h>

static bool cstr_match(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] && s2[i]) {
        if (s1[i] != s2[i]) return false;
        i++;
    }
    return s1[i] == s2[i];
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

        if (cmd_line == NULL) continue;
        if (str_is_empty(cmd_line)) { release(cmd_line); continue; }

        /* Split arguments by spaces */
        char* line_buf = (char*)str_to_cstr(cmd_line);
        char* argv[10];
        int argc = 0;
        char* p = line_buf;

        while (*p && argc < 10) {
            while (*p == ' ') *p++ = '\0';
            if (*p == '\0') break;
            argv[argc++] = p;
            while (*p && *p != ' ') p++;
        }

        if (argc == 0) { release(cmd_line); continue; }

        /* Dispatcher */
        if (cstr_match(argv[0], "ls")) {
            rsl_ls(curdir);
        }
        else if (cstr_match(argv[0], "cat")) {
            if (argc > 1) {
                void* path = str_create(argv[1]);
                rsl_cat(path);
                release(path);
            } else {
                print("Usage: cat <file>\n");
            }
        }
        else if (cstr_match(argv[0], "write")) {
            if (argc > 1) {
                void* path = str_create(argv[1]);
                void* content = input("Enter Content: ");
                if (content) {
                    rsl_write(path, content);
                    release(content);
                }
                release(path);
            } else {
                void* filename = input("Enter Filename: ");
                if (filename) {
                    void* content = input("Enter Content: ");
                    if (content) {
                        rsl_write(filename, content);
                        release(content);
                    }
                    release(filename);
                }
            }
        }
        else if (cstr_match(argv[0], "cd")) {
            if (argc > 1) {
                release(curdir);
                curdir = str_create(argv[1]);
                rsl_cd(curdir);
            } else {
                print("Usage: cd <path>\n");
            }
        }
        else if (cstr_match(argv[0], "color")) {
            if (argc > 2) {
                set_color(name_to_color(argv[1]), name_to_color(argv[2]));
                print("Color Updated.\n");
            } else {
                void* fg_name = input("Enter FG Color: ");
                if (fg_name) {
                    void* bg_name = input("Enter BG Color: ");
                    if (bg_name) {
                        set_color(name_to_color(str_to_cstr(fg_name)), name_to_color(str_to_cstr(bg_name)));
                        release(bg_name);
                        print("Color Updated.\n");
                    }
                    release(fg_name);
                }
            }
        }
        else if (cstr_match(argv[0], "help")) {
            print("Available Commands:\n");
            print("ls          - List disks/partitions/files\n");
            print("cd <path>   - Change directory context\n");
            print("cat <file>  - Read file content\n");
            print("write <f>   - File creation\n");
            print("color <f> <b>- Change console colors\n");
            print("help        - Show this menu\n");
            print("exit        - Terminate shell\n");
        }
        else if (cstr_match(argv[0], "exit")) {
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
