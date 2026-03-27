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
    set_color(LIGHT_CYAN, BLACK);
    print("  _____ _______ ______ _____ _    _    ____   _____     ___  \n");
    print(" |  __ \\__   __|  ____/ ____| |  | |  / __ \\ / ____|   |__ \\ \n");
    print(" | |__) | | |  | |__ | |    | |__| | | |  | | (_____  __  ) |\n");
    print(" |  _  /  | |  |  __|| |    |  __  | | |  | |\\___ \\ \\/ / / / \n");
    print(" | | \\ \\  | |  | |___| |____| |  | | | |__| |____) >  < / /_ \n");
    print(" |_|  \\_\\ |_|  |______\\_____|_|  |_|  \\____/|_____/_/\\_\\____|\n");
    print("\n[ OSx2 Sovereign ] Build Success.\n");

    set_color(GREEN, BLACK);
    void* curdir = str_create("/");

    while (1) {
        void* suffix = str_create("OS2>");
        void* prompt = str_concat(curdir, suffix);
        release(suffix);

        void* cmd_line = input(str_to_cstr(prompt));
        release(prompt);

        if (cmd_line == NULL) continue;
        if (str_is_empty(cmd_line)) { release(cmd_line); continue; }

        /* Multi-Arg Parser (Treats each space as an argument separator) */
        char* line_buf = (char*)str_to_cstr(cmd_line);
        char* argv[16];
        int argc = 0;
        char* p = line_buf;

        while (*p && argc < 16) {
            while (*p == ' ') *p++ = '\0'; /* Skip leading spaces */
            if (*p == '\0') break;
            argv[argc++] = p;
            while (*p && *p != ' ') p++; /* Skip non-spaces */
        }

        if (argc == 0) { release(cmd_line); continue; }

        /* Dispatcher */
        if (cstr_match(argv[0], "ls")) {
            rsl_ls(curdir);
        }
        else if (cstr_match(argv[0], "echo")) {
            for (int i = 1; i < argc; i++) {
                print(argv[i]);
                if (i < argc - 1) print(" ");
            }
            print("\n");
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
                void* new_path;
                if (cstr_match(argv[1], "/")) {
                    if (cstr_match(str_to_cstr(curdir), "/")) {
                        release(cmd_line);
                        continue;
                    }
                    new_path = str_create("/");
                } else if (cstr_match(argv[1], "..")) {
                    /* Basic parent directory traversal */
                    const char* cur = str_to_cstr(curdir);
                    if (cstr_match(cur, "/")) {
                        new_path = str_create("/");
                    } else {
                        /* Simplified: return to / if not at / */
                        new_path = str_create("/");
                    }
                } else {
                    new_path = str_create(argv[1]);
                }

                if (rsl_exists(new_path)) {
                    release(curdir);
                    curdir = new_path;
                    rsl_cd(curdir);
                } else {
                    print("Error: Path does not exist.\n");
                    release(new_path);
                }
            } else {
                print("Usage: cd <path>\n");
            }
        }
        else if (cstr_match(argv[0], "mkdir")) {
            if (argc > 1) {
                void* path = str_create(argv[1]);
                rsl_mkdir(path);
                release(path);
            } else {
                print("Usage: mkdir <name>\n");
            }
        }
        else if (cstr_match(argv[0], "mount")) {
            if (argc > 1) {
                void* path = str_create(argv[1]);
                rsl_mount(path);
                release(path);
            } else {
                print("Usage: mount <path>\n");
            }
        }
        else if (cstr_match(argv[0], "format")) {
            if (argc > 1) {
                void* path = str_create(argv[1]);
                rsl_format(path);
                release(path);
            } else {
                print("Usage: format <path>\n");
            }
        }
        else if (cstr_match(argv[0], "rmdir")) {
            if (argc > 1) {
                void* path = str_create(argv[1]);
                rsl_rmdir(path);
                release(path);
            } else {
                print("Usage: rmdir <name>\n");
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
            print("OSx2 Limemade RSL Shell Commands:\n");
            print("---------------------------------\n");
            print("ls [path]      - List Sovereign disks (at /), ATAPI nodes, or directories\n");
            print("cd <path>      - Change to a new Sovereign path (e.g., 0:/)\n");
            print("cat <file>     - Display contents of a Sovereign file\n");
            print("echo <text>    - Print text to the screen\n");
            print("write <file>   - Create or overwrite a file with interactive input\n");
            print("mkdir <name>   - Create a new Sovereign directory\n");
            print("rmdir <name>   - Remove a Sovereign directory or file\n");
            print("mount <path>   - Mount a Sovereign volume (e.g., 0:)\n");
            print("format <path>  - Physically format a drive (e.g., 0:)\n");
            print("color <fg> <bg>- Update console colors (e.g., color green black)\n");
            print("help           - Show this command list\n");
            print("exit           - Terminate the RSL shell session\n");
        }
        else if (cstr_match(argv[0], "run")) {
            if (argc > 1) {
                void rsl_execute_stream(const char* path);
                rsl_execute_stream(argv[1]);
            } else {
                print("Usage: run <path>\n");
            }
        }
        else if (cstr_match(argv[0], "exit")) {
            void rsl_shutdown(void);
            rsl_shutdown();
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
