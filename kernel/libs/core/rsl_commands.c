#include <include/rsl.h>
#include <include/vfs.h>
#include <kernel/unice64/task.h>
#include <kernel/libs/storage/fatfs/ff.h>
#include <kernel/libs/core/services.h>
#include <include/cmdlets.h>

void vga_print(const char* fmt, ...);

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

void rsl_ls(void* path) { vfs_ls(path); }
void rsl_cat(void* path) { vfs_cat(path); }
void rsl_write(void* path, void* content) { vfs_write_dispatch(path, content); }
void rsl_mkdir(void* path) { vfs_mkdir(path); }
void rsl_rmdir(void* path) { vfs_rmdir(path); }
bool rsl_exists(void* path) { return vfs_exists(path); }
void rsl_mount(void* path) { (void)path; print("[RSL] Mount not implemented.\n"); }
void rsl_format(void* path) { (void)path; print("[RSL] Format not implemented.\n"); }
void rsl_stamp(void* path) { (void)path; print("[RSL] Stamp not implemented.\n"); }
void rsl_scan(void) { print("[RSL] Hardware scan...\n"); }
void rsl_eject(void* path) { (void)path; print("[RSL] Eject not implemented.\n"); }

static void* curdir = NULL;

static bool cstr_match_local(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] && s2[i]) {
        if (s1[i] != s2[i]) return false;
        i++;
    }
    return s1[i] == s2[i];
}

static void* resolve_path_local(void* cd, const char* arg) {
    if (arg[0] == '/') return str_create(arg);
    if (cstr_match_local(str_to_cstr(cd), "/")) return str_concat(cd, str_create(arg));
    void* s1 = str_concat(cd, str_create("/"));
    void* s2 = str_concat(s1, str_create(arg));
    release(s1); return s2;
}

void rsl_execute_command(char* line) {
    if (!curdir) curdir = str_create("/");
    if (!line || line[0] == '\0') return;

    /* CMD-LETS Bridge */
    static bool in_cmdlets = false;
    if (!in_cmdlets) {
        in_cmdlets = true;
        cmdlets_execute_line(line);
        in_cmdlets = false;
        return;
    }

    char* argv[16];
    int argc = 0;
    char* p = line;

    while (*p && argc < 16) {
        while (*p == ' ' || *p == '\r' || *p == '\t' || *p == '\n') *p++ = '\0';
        if (*p == '\0') break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\r' && *p != '\t' && *p != '\n') p++;
    }

    if (argc == 0) return;

    if (cstr_match_local(argv[0], "ls")) {
        void* path = (argc > 1) ? resolve_path_local(curdir, argv[1]) : curdir;
        rsl_ls(path); if (argc > 1) release(path);
    } else if (cstr_match_local(argv[0], "cd")) {
        if (argc > 1) {
            void* new_path = resolve_path_local(curdir, argv[1]);
            if (rsl_exists(new_path)) {
                const char* nps = str_to_cstr(new_path);
                int len = 0; while(nps[len]) len++;
                if (len > 0 && nps[len-1] != '/') {
                    void* slash = str_create("/");
                    void* fixed = str_concat(new_path, slash);
                    release(slash); release(new_path);
                    new_path = fixed;
                }
                release(curdir); curdir = new_path;
            } else {
                print("Error: Path not found.\n"); release(new_path);
            }
        }
    } else if (cstr_match_local(argv[0], "cat")) {
        if (argc > 1) {
            void* p = resolve_path_local(curdir, argv[1]);
            rsl_cat(p); release(p);
            print("\n");
        }
    } else if (cstr_match_local(argv[0], "write")) {
        if (argc > 1) {
            void* p = resolve_path_local(curdir, argv[1]);
            void* content = (argc > 2) ? str_create(argv[2]) : input("Enter Content: ");
            if (content) {
                rsl_write(p, content);
                release(content);
            }
            release(p);
        }
    } else if (cstr_match_local(argv[0], "mkdir")) {
        if (argc > 1) {
            void* p = resolve_path_local(curdir, argv[1]);
            rsl_mkdir(p); release(p);
        }
    } else if (cstr_match_local(argv[0], "rmdir")) {
        if (argc > 1) {
            void* p = resolve_path_local(curdir, argv[1]);
            rsl_rmdir(p); release(p);
        }
    } else if (cstr_match_local(argv[0], "echo")) {
        for (int i = 1; i < argc; i++) {
            print(argv[i]); if (i < argc - 1) print(" ");
        }
        print("\n");
    } else if (cstr_match_local(argv[0], "shutdown")) {
        void rsl_shutdown(void);
        rsl_shutdown();
    } else if (cstr_match_local(argv[0], "exit")) {
        task_t* current = get_current_task();
        if (current) current->state = TASK_ZOMBIE;
        sys_yield();
    } else if (cstr_match_local(argv[0], "run")) {
        if (argc > 1) {
            int rsl_execute_stream(const char* path);
            rsl_execute_stream(argv[1]);
        }
    } else if (argv[0][0] == '.' && argv[0][1] == '/') {
        /* Direct Execution: ./program */
        void sovereign_request_submit(system_request_t* req);
        system_request_t req = {
            .type = REQ_APP_SPAWN,
            .path = resolve_path_local(curdir, &argv[0][2]),
            .done = false
        };
        sovereign_request_submit(&req);
        release(req.path);
    } else if (cstr_match_local(argv[0], "exec")) {
        if (argc > 1) {
            void sovereign_request_submit(system_request_t* req);
            system_request_t req = {
                .type = REQ_APP_SPAWN,
                .path = resolve_path_local(curdir, argv[1]),
                .done = false
            };
            sovereign_request_submit(&req);
            release(req.path);
        }
    } else if (cstr_match_local(argv[0], "STDE")) {
        void sovereign_request_submit(system_request_t* req);
        system_request_t req = {
            .type = REQ_APP_SPAWN,
            .path = str_create("BOOT:/bin/wm.bin"),
            .done = false
        };
        sovereign_request_submit(&req);
        release(req.path);
    } else {
        print("Unknown Command.\n");
    }
}

void* rsl_get_curdir(void) {
    if (!curdir) curdir = str_create("/");
    retain(curdir);
    return curdir;
}
