#include <include/rsl.h>

static bool cstr_match(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] && s2[i]) {
        if (s1[i] != s2[i]) return false;
        i++;
    }
    return s1[i] == s2[i];
}

static bool is_absolute(const char* path) {
    /* Check if it has a ':' (SATA0:/) or starts with '/' */
    int i = 0;
    while (path[i]) {
        if (path[i] == ':') return true;
        i++;
    }
    return path[0] == '/';
}

static void* resolve_path(void* curdir, const char* arg) {
    if (is_absolute(arg)) return str_create(arg);

    const char* cd = str_to_cstr(curdir);
    if (cstr_match(cd, "/")) {
        /* If at root and arg isn't absolute, prepend nothing (disk list context) */
        return str_create(arg);
    }

    return str_concat(curdir, str_create(arg));
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
        bool is_safe = rsl_safe_mode();
        if (is_safe) {
            set_color(YELLOW, BLACK);
            print("[SAFE MODE] ");
            set_color(GREEN, BLACK);
        }

        if (cstr_match(argv[0], "ls")) {
            if (argc > 1) {
                void* path = resolve_path(curdir, argv[1]);
                rsl_ls(path);
                release(path);
            } else {
                rsl_ls(curdir);
            }
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
                void* path = resolve_path(curdir, argv[1]);
                rsl_cat(path);
                release(path);
                print("\n"); /* Ensure prompt starts on new line */
            } else {
                print("Usage: cat <file>\n");
            }
        }
        else if (cstr_match(argv[0], "write")) {
            if (is_safe) { print("Error: Write commands disabled in Safe Mode.\n"); release(cmd_line); continue; }
            if (cstr_match(str_to_cstr(curdir), "/")) { print("Error: Access Denied at root. Access a drive (e.g. cd BOOT:/).\n"); release(cmd_line); continue; }
            if (argc > 1) {
                void* path = resolve_path(curdir, argv[1]);
                void* content = input("Enter Content: ");
                if (content) {
                    rsl_write(path, content);
                    release(content);
                }
                release(path);
            } else {
                void* arg_name = input("Enter Filename: ");
                if (arg_name) {
                    void* path = resolve_path(curdir, str_to_cstr(arg_name));
                    void* content = input("Enter Content: ");
                    if (content) {
                        rsl_write(path, content);
                        release(content);
                    }
                    release(path);
                    release(arg_name);
                }
            }
        }
        else if (cstr_match(argv[0], "cd")) {
            if (argc > 1) {
                void* new_path;
                if (cstr_match(argv[1], "/")) {
                    new_path = str_create("/");
                } else if (cstr_match(argv[1], "..")) {
                    /* Basic parent directory traversal */
                    const char* cur = str_to_cstr(curdir);
                    if (cstr_match(cur, "/")) {
                        new_path = str_create("/");
                    } else {
                        /* Parent directory implementation */
                        int last_slash = -1;
                        int len = 0;
                        while(cur[len]) {
                            if (cur[len] == '/' && cur[len+1] != '\0') last_slash = len;
                            len++;
                        }
                        if (last_slash == -1) new_path = str_create("/");
                        else {
                            char buf[256];
                            for(int k=0; k<=last_slash; k++) buf[k] = cur[k];
                            buf[last_slash+1] = '\0';
                            new_path = str_create(buf);
                        }
                    }
                } else {
                    new_path = resolve_path(curdir, argv[1]);
                }

                bool valid = cstr_match(str_to_cstr(new_path), "/") || rsl_exists(new_path);

                /* Support Mapping: Allow cd to Drive ID (e.g. 0:/) to resolve to name (e.g. BOOT:/) */
                if (!valid) {
                    const char* np = str_to_cstr(new_path);
                    if (np[0] >= '0' && np[0] <= '9' && np[1] == ':' && np[2] == '/') {
                        /* This is a drive ID path, check if it's currently mounted and exists */
                        valid = true;
                    }
                }

                if (valid) {
                    /* Ensure directories end with / */
                    const char* nps = str_to_cstr(new_path);
                    int len = 0; while(nps[len]) len++;
                    if (len > 0 && nps[len-1] != '/') {
                        void* slash = str_create("/");
                        void* fixed = str_concat(new_path, slash);
                        release(slash);
                        release(new_path);
                        new_path = fixed;
                    }

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
            if (is_safe) { print("Error: Directory commands disabled in Safe Mode.\n"); release(cmd_line); continue; }
            if (cstr_match(str_to_cstr(curdir), "/")) { print("Error: Access Denied at root.\n"); release(cmd_line); continue; }
            if (argc > 1) {
                void* path = resolve_path(curdir, argv[1]);
                rsl_mkdir(path);
                release(path);
            } else {
                print("Usage: mkdir <name>\n");
            }
        }
        else if (cstr_match(argv[0], "mount")) {
            if (argc > 1) {
                const char* p = argv[1];
                bool has_vol = false;
                for(int k=0; p[k]; k++) if(p[k] == ':' && p[k+1] == '/') has_vol = true;

                if (!has_vol) {
                    print("Error: Volume Prefix Fault. Paths must contain ':/' (e.g. SATA0:/)\n");
                } else {
                    void* path = str_create(p);
                    rsl_mount(path);
                    release(path);
                }
            } else {
                print("Usage: mount <drive_id:/path>\n");
            }
        }
        else if (cstr_match(argv[0], "format")) {
            if (argc > 1) {
                const char* p = argv[1];
                bool has_vol = false;
                for(int k=0; p[k]; k++) if(p[k] == ':' && p[k+1] == '/') has_vol = true;

                if (!has_vol) {
                    print("Error: Volume Prefix Fault. Paths must contain ':/' (e.g. SATA0:/)\n");
                } else {
                    void* path = str_create(p);
                    rsl_format(path);
                    release(path);
                }
            } else {
                print("Usage: format <drive_id:/path>\n");
            }
        }
        else if (cstr_match(argv[0], "stamp")) {
            if (argc > 1) {
                void* path = str_create(argv[1]);
                rsl_stamp(path);
                release(path);
            } else {
                print("Usage: stamp <drive_id:>\n");
            }
        }
        else if (cstr_match(argv[0], "eject")) {
            if (argc > 1) {
                void* path = str_create(argv[1]);
                rsl_eject(path);
                release(path);
            } else {
                print("Usage: eject <drive_id:>\n");
            }
        }
        else if (cstr_match(argv[0], "rmdir")) {
            if (is_safe) { print("Error: Directory commands disabled in Safe Mode.\n"); release(cmd_line); continue; }
            if (cstr_match(str_to_cstr(curdir), "/")) { print("Error: Access Denied at root.\n"); release(cmd_line); continue; }
            if (argc > 1) {
                void* path = resolve_path(curdir, argv[1]);
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
            print("ls [path]      - List disks at / or files in context\n");
            print("cd <path>      - Change directory. Use '..' for up, '/' for root.\n");
            print("cat <file>     - View file content. Returns to shell after.\n");
            print("write <file>   - Create/Overwrite file in current directory.\n");
            print("mkdir <name>   - Create new directory in current directory.\n");
            print("rmdir <name>   - Delete directory or file.\n");
            print("echo <text>    - Print text to output.\n");
            print("mount <id>     - Mount disk ID (e.g. 0) to SATAx:.\n");
            print("format <id>    - Format disk ID to Sovereign FAT32.\n");
            print("run <script>   - Execute RSL script from disk.\n");
            print("color <fg> <bg>- Set colors (cyan, green, red, white, etc).\n");
            print("help           - Show this command list.\n");
            print("exit           - Shutdown OSx2 Limemade.\n");
            print("\nPaths: Drives use prefix (BOOT:/). Relative paths use curdir.\n");
        }
        else if (cstr_match(argv[0], "run")) {
            if (argc > 1) {
                void rsl_execute_stream(const char* path);
                rsl_execute_stream(argv[1]);
            } else {
                print("Usage: run <path>\n");
            }
        }
        else if (cstr_match(argv[0], "scan")) {
            rsl_scan();
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
