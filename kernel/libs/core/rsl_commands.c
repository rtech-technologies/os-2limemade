#include <include/rsl.h>
#include <include/vfs.h>
#include <kernel/libs/storage/fatfs/ff.h>
#include <kernel/unice64/task.h>

int get_hw_disk_count(void);
int get_connect_disk_count(void);
void vga_print(const char* fmt, ...);
size_t slab_get_usage(int id);
void rsl_execute_stream(const char* path);
void rsl_shutdown(void);
void get_task_info(int idx, uint32_t* id, const char** state, uint32_t* slab);

/* Internal FS Bridge for VFS */
void internal_fs_ls(void* path, void* priv) {
    DIR dp; FILINFO fno;
    FATFS* fs = (FATFS*)priv;
    if (f_opendir(fs, &dp, str_to_cstr(path)) == FR_OK) {
        while (f_readdir(&dp, &fno) == FR_OK && fno.fname[0] != 0) {
            print(fno.fname); print("\n");
        }
    }
}

void internal_fs_cat(void* path, void* priv) {
    FIL fp; uint32_t br; char buf[512];
    FATFS* fs = (FATFS*)priv;
    if (f_open(fs, &fp, str_to_cstr(path), FA_READ) == FR_OK) {
        while (f_read(&fp, buf, 511, &br) == FR_OK && br > 0) {
            buf[br] = '\0'; print(buf);
        }
        f_close(&fp);
    }
}

void internal_fs_write(void* path, void* content, void* priv) {
    FIL fp; uint32_t bw;
    FATFS* fs = (FATFS*)priv;
    if (f_open(fs, &fp, str_to_cstr(path), FA_WRITE|FA_CREATE_ALWAYS) == FR_OK) {
        f_write(&fp, str_to_cstr(content), (uint32_t)str_len(content), &bw);
        f_close(&fp);
    }
}

void internal_fs_mkdir(void* path, void* priv) {
    FATFS* fs = (FATFS*)priv;
    f_mkdir(fs, str_to_cstr(path));
}

void internal_fs_rmdir(void* path, void* priv) {
    FATFS* fs = (FATFS*)priv;
    f_unlink(fs, str_to_cstr(path));
}

bool internal_fs_exists(void* path, void* priv) {
    FILINFO fno;
    FATFS* fs = (FATFS*)priv;
    return f_stat(fs, str_to_cstr(path), &fno) == FR_OK;
}

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

static bool is_absolute(const char* path) {
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
    if (cstr_match(cd, "/")) return str_create(arg);
    return str_concat(curdir, str_create(arg));
}

void rsl_dispatch_command(char* line, void** curdir_ptr, bool* is_safe_ptr) {
    char* argv[16];
    int argc = 0;
    char* p = line;

    while (*p && argc < 16) {
        while (*p == ' ') *p++ = '\0';
        if (*p == '\0') break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
    }

    if (argc == 0) return;

    if (*is_safe_ptr) {
        set_color(YELLOW, BLACK);
        print("[SAFE MODE] ");
        set_color(GREEN, BLACK);
    }

    if (cstr_match(argv[0], "ls")) {
        if (argc > 1) {
            void* path = resolve_path(*curdir_ptr, argv[1]);
            rsl_ls(path);
            release(path);
        } else {
            rsl_ls(*curdir_ptr);
        }
    } else if (cstr_match(argv[0], "echo")) {
        for (int i = 1; i < argc; i++) {
            print(argv[i]);
            if (i < argc - 1) print(" ");
        }
        print("\n");
    } else if (cstr_match(argv[0], "cat")) {
        if (argc > 1) {
            void* path = resolve_path(*curdir_ptr, argv[1]);
            rsl_cat(path);
            release(path);
            print("\n");
        } else {
            print("Usage: cat <file>\n");
        }
    } else if (cstr_match(argv[0], "cd")) {
        if (argc > 1) {
            void* new_path;
            if (cstr_match(argv[1], "/")) new_path = str_create("/");
            else if (cstr_match(argv[1], "..")) {
                const char* cur = str_to_cstr(*curdir_ptr);
                if (cstr_match(cur, "/")) new_path = str_create("/");
                else {
                    int last_slash = -1, len = 0;
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
            } else new_path = resolve_path(*curdir_ptr, argv[1]);

            if (cstr_match(str_to_cstr(new_path), "/") || rsl_exists(new_path)) {
                const char* nps = str_to_cstr(new_path);
                int len = 0; while(nps[len]) len++;
                if (len > 0 && nps[len-1] != '/') {
                    void* slash = str_create("/");
                    void* fixed = str_concat(new_path, slash);
                    release(slash); release(new_path);
                    new_path = fixed;
                }
                release(*curdir_ptr);
                *curdir_ptr = new_path;
                rsl_cd(*curdir_ptr);
            } else {
                print("Error: Path does not exist.\n");
                release(new_path);
            }
        }
    } else if (cstr_match(argv[0], "mkdir")) {
        if (*is_safe_ptr) { print("Error: Write disabled in Safe Mode.\n"); return; }
        if (argc > 1) {
            void* path = resolve_path(*curdir_ptr, argv[1]);
            rsl_mkdir(path);
            release(path);
        }
    } else if (cstr_match(argv[0], "format")) {
        if (argc > 1) {
            void* path = str_create(argv[1]);
            rsl_format(path);
            release(path);
        }
    } else if (cstr_match(argv[0], "run")) {
        if (argc > 1) rsl_execute_stream(argv[1]);
    } else if (cstr_match(argv[0], "color")) {
        if (argc > 2) set_color(name_to_color(argv[1]), name_to_color(argv[2]));
    } else if (cstr_match(argv[0], "exit")) {
        rsl_shutdown();
    } else if (cstr_match(argv[0], "help")) {
        print("Commands: ls, echo, cat, cd, mkdir, format, run, color, help, exit, settings, debug-dump, scan\n");
    } else if (cstr_match(argv[0], "settings")) {
        rsl_settings();
    } else if (cstr_match(argv[0], "debug-dump")) {
        rsl_debug_dump();
    } else if (cstr_match(argv[0], "scan")) {
        rsl_scan();
    } else {
        print("Unknown command: "); print(argv[0]); print("\n");
    }
}

void rsl_settings(void) {
    print("\n--- OSx2 Sovereign Settings ---\n");
    print("1. Memory Telemetry\n");
    print("2. Task Telemetry\n");
    print("3. System Identity\n");

    void* choice = input("\nSelect Option (1-3): ");
    if (!choice) return;

    if (str_match(choice, "1")) {
        for (int i = 0; i < 4; i++) {
            vga_print("Slab %d: %d / 4194304 bytes used.\n", i, (int)slab_get_usage(i));
        }
    } else if (str_match(choice, "2")) {
        print("Tasks:\nID  STATE   SLAB\n");
        int count = get_task_count();
        for (int i = 0; i < count; i++) {
            uint32_t tid, tslab;
            const char* tstate;
            get_task_info(i, &tid, &tstate, &tslab);
            vga_print("%d   %s   %d\n", tid, tstate, tslab);
        }
    } else if (str_match(choice, "3")) {
        print("OS Identity: OSx2 Sovereign (Limemade Build)\n");
        print("Foundation: Active-Relay Round Robin\n");
        print("Storage: Mechanical Truth AHCI/ATAPI Bridge\n");
    }
    release(choice);
}

void rsl_debug_dump(void) {
    uint64_t cr3; __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    vga_print("[DEBUG] CR3: 0x%x\n", (uint32_t)cr3);
    task_t* tcb = get_current_task();
    vga_print("[DEBUG] Current TCB: 0x%x\n", (uint64_t)tcb);
}

void rsl_format(void* path) {
    const char* p = str_to_cstr(path);
    int drive = p[0] - '0';
    if (f_mkfs(drive) == FR_OK) print("Format successful.\n");
}

void rsl_scan(void) {
    void ahci_scan_remaining(void);
    ahci_scan_remaining();
}

void rsl_ls(void* path) { vfs_ls(path); }
void rsl_cat(void* path) { vfs_cat(path); }
void rsl_cd(void* path) { vfs_cd(path); }
void rsl_mkdir(void* path) { vfs_mkdir(path); }
void rsl_rmdir(void* path) { vfs_rmdir(path); }
bool rsl_exists(void* path) { return vfs_exists(path); }
void rsl_mount(void* path) { (void)path; /* Simplified */ }
void rsl_stamp(void* path) { (void)path; }
void rsl_eject(void* path) { (void)path; }
