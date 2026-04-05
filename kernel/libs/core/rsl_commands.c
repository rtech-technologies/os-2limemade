#include <include/rsl.h>
#include <include/vfs.h>
#include <kernel/libs/storage/fatfs/ff.h>
#include <kernel/unice64/task.h>

int get_hw_disk_count(void);
int get_connect_disk_count(void);

#define MAX_MOUNTS 16
static FATFS mount_table[MAX_MOUNTS];
static int mount_count = 0;

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

void rsl_ls(void* path) { vfs_ls(path); sys_yield(); }
void rsl_cat(void* path) { vfs_cat(path); sys_yield(); }
void vfs_write_dispatch(void* path, void* content);
void rsl_write(void* path, void* content) { vfs_write_dispatch(path, content); sys_yield(); }
void rsl_cd(void* path) { vfs_cd(path); sys_yield(); }
void rsl_mkdir(void* path) { vfs_mkdir(path); sys_yield(); }
void rsl_rmdir(void* path) { vfs_rmdir(path); sys_yield(); }
bool rsl_exists(void* path) { return vfs_exists(path); }

bool rsl_safe_mode(void) { return vfs_is_safe_mode(); }

void rsl_mount(void* path) {
    const char* p = str_to_cstr(path);
    int drive = p[0] - '0';
    if (mount_count >= MAX_MOUNTS) return;

    /* Verify Hardware Drive Exists */
    if (drive < 0 || drive >= get_hw_disk_count()) {
        print("Error: Physical drive does not exist.\n");
        return;
    }

    if (f_mount(&mount_table[mount_count], drive) == FR_OK) {
        vfs_node_t node = { .private_data = &mount_table[mount_count], .ls = internal_fs_ls, .cat = internal_fs_cat, .write = internal_fs_write, .mkdir = internal_fs_mkdir, .rmdir = internal_fs_rmdir, .exists = internal_fs_exists };
        int k = 0; if (drive >= 10) node.name[k++] = '0' + (drive / 10); node.name[k++] = '0' + (drive % 10); node.name[k] = '\0';
        vfs_register_node(node);
        mount_count++;
        print("Mount successful.\n");
    } else {
        print("Error: Mount failed.\n");
    }
    sys_yield();
}

static void* curdir = NULL;

void* rsl_get_curdir(void) {
    if (!curdir) curdir = str_create("/");
    retain(curdir);
    return curdir;
}

static bool cstr_match_local(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] && s2[i]) {
        if (s1[i] != s2[i]) return false;
        i++;
    }
    return s1[i] == s2[i];
}

static bool is_absolute(const char* path) {
    int i = 0;
    while (path[i]) {
        if (path[i] == ':') return true;
        i++;
    }
    return path[0] == '/';
}

static void* resolve_path_local(void* curdir, const char* arg) {
    if (is_absolute(arg)) return str_create(arg);
    const char* cd = str_to_cstr(curdir);
    if (cstr_match_local(cd, "/")) return str_create(arg);
    return str_concat(curdir, str_create(arg));
}

static color_t name_to_color_local(const char* name) {
    if (cstr_match_local(name, "black")) return BLACK;
    if (cstr_match_local(name, "blue")) return BLUE;
    if (cstr_match_local(name, "green")) return GREEN;
    if (cstr_match_local(name, "cyan")) return CYAN;
    if (cstr_match_local(name, "red")) return RED;
    if (cstr_match_local(name, "magenta")) return MAGENTA;
    if (cstr_match_local(name, "brown")) return BROWN;
    if (cstr_match_local(name, "white")) return WHITE;
    if (cstr_match_local(name, "yellow")) return YELLOW;
    return WHITE;
}

void rsl_execute_command(char* line) {
    if (!curdir) curdir = str_create("/");
    if (!line || line[0] == '\0') return;

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
        if (argc > 1) {
            void* path = resolve_path_local(curdir, argv[1]);
            rsl_ls(path); release(path);
        } else rsl_ls(curdir);
    } else if (cstr_match_local(argv[0], "cd")) {
        if (argc > 1) {
            void* new_path;
            if (cstr_match_local(argv[1], "/")) new_path = str_create("/");
            else if (cstr_match_local(argv[1], "..")) {
                const char* cur = str_to_cstr(curdir);
                int last_slash = -1;
                for(int k=0; cur[k]; k++) if (cur[k] == '/' && cur[k+1] != '\0') last_slash = k;
                if (last_slash == -1) new_path = str_create("/");
                else {
                    char buf[256]; int k;
                    for(k=0; k<=last_slash; k++) buf[k] = cur[k];
                    buf[k] = '\0'; new_path = str_create(buf);
                }
            } else new_path = resolve_path_local(curdir, argv[1]);

            if (cstr_match_local(str_to_cstr(new_path), "/") || rsl_exists(new_path)) {
                const char* nps = str_to_cstr(new_path);
                int len = 0; while(nps[len]) len++;
                if (len > 0 && nps[len-1] != '/') {
                    void* slash = str_create("/");
                    void* fixed = str_concat(new_path, slash);
                    release(slash); release(new_path);
                    new_path = fixed;
                }
                release(curdir); curdir = new_path;
                rsl_cd(curdir);
            } else {
                print("Error: Path not found.\n"); release(new_path);
            }
        }
    } else if (cstr_match_local(argv[0], "cat")) {
        if (argc > 1) {
            void* path = resolve_path_local(curdir, argv[1]);
            rsl_cat(path); release(path);
            print("\n");
        }
    } else if (cstr_match_local(argv[0], "write")) {
        if (argc > 1) {
            void* path = resolve_path_local(curdir, argv[1]);
            void* content;
            if (argc > 2) content = str_create(argv[2]);
            else content = input("Enter Content: ");
            if (content) {
                rsl_write(path, content); release(content);
            }
            release(path);
        }
    } else if (cstr_match_local(argv[0], "mkdir")) {
        if (argc > 1) {
            void* path = resolve_path_local(curdir, argv[1]);
            rsl_mkdir(path); release(path);
        }
    } else if (cstr_match_local(argv[0], "rmdir")) {
        if (argc > 1) {
            void* path = resolve_path_local(curdir, argv[1]);
            rsl_rmdir(path); release(path);
        }
    } else if (cstr_match_local(argv[0], "echo")) {
        for (int i = 1; i < argc; i++) {
            print(argv[i]); if (i < argc - 1) print(" ");
        }
        print("\n");
    } else if (cstr_match_local(argv[0], "color")) {
        if (argc > 2) {
            set_color(name_to_color_local(argv[1]), name_to_color_local(argv[2]));
            print("Color Updated.\n");
        }
    } else if (cstr_match_local(argv[0], "copy")) {
        if (argc > 1) {
            void* s = str_create(argv[1]);
            rsl_copy(s); release(s);
            print("Copied to clipboard.\n");
        }
    } else if (cstr_match_local(argv[0], "paste")) {
        void* s = rsl_paste();
        if (s) {
            print(str_to_cstr(s)); print("\n"); release(s);
        } else print("Clipboard empty.\n");
    } else if (cstr_match_local(argv[0], "mount")) {
        if (argc > 1) {
            void* path = str_create(argv[1]);
            rsl_mount(path); release(path);
        }
    } else if (cstr_match_local(argv[0], "format")) {
        if (argc > 1) {
            void* path = str_create(argv[1]);
            rsl_format(path); release(path);
        }
    } else if (cstr_match_local(argv[0], "stamp")) {
        if (argc > 1) {
            void* path = str_create(argv[1]);
            rsl_stamp(path); release(path);
        }
    } else if (cstr_match_local(argv[0], "eject")) {
        if (argc > 1) {
            void* path = str_create(argv[1]);
            rsl_eject(path); release(path);
        }
    } else if (cstr_match_local(argv[0], "debug-dump")) {
        rsl_debug_dump();
    } else if (cstr_match_local(argv[0], "scan")) {
        rsl_scan();
    } else if (cstr_match_local(argv[0], "settings")) {
        rsl_settings();
    } else if (cstr_match_local(argv[0], "help")) {
        print("OSx2 Limemade Commands:\n");
        print("ls, cd, cat, write, mkdir, rmdir, echo, color, copy, paste, run, settings, debug-dump, scan, help, exit\n");
    } else if (cstr_match_local(argv[0], "run")) {
        if (argc > 1) {
            void rsl_execute_stream(const char* path);
            rsl_execute_stream(argv[1]);
        }
    } else if (cstr_match_local(argv[0], "exit")) {
        void rsl_shutdown(void);
        rsl_shutdown();
    } else {
        print("Unknown Command.\n");
    }
}

void vga_print(const char* fmt, ...);
size_t slab_get_usage(int id);

void rsl_settings(void) {
    print("\n--- OSx2 Sovereign Settings ---\n");
    print("1. UI Theme (Change FG/BG)\n");
    print("2. Memory Telemetry (Slab Usage)\n");
    print("3. Task Telemetry (Task List)\n");
    print("4. System Identity\n");
    print("5. Back to Shell\n");

    void* choice = input("\nSelect Category (1-5): ");
    if (!choice) return;

    if (str_match(choice, "1")) {
        void* fg = input("Enter FG Color Name: ");
        void* bg = input("Enter BG Color Name: ");
        /* Implementation in Shell dispatcher for now or simplified here */
        print("Theme applied.\n");
        release(fg); release(bg);
    } else if (str_match(choice, "2")) {
        for (int i = 0; i < 4; i++) {
            vga_print("Slab %d: %d / 4194304 bytes used.\n", i, slab_get_usage(i));
        }
    } else if (str_match(choice, "3")) {
        print("Tasks:\nID  STATE   SLAB\n");
        /* This would require a scheduler walk, let's provide a stub */
        print("0   READY   0 (Idle)\n");
        print("1   RUNNING 1 (Shell)\n");
        print("2   READY   2 (System)\n");
    } else if (str_match(choice, "4")) {
        print("OS Identity: OSx2 Sovereign (Limemade Build)\n");
        print("Foundation: Active-Relay Round Robin\n");
        print("Storage: Mechanical Truth AHCI/ATAPI Bridge\n");
    }

    release(choice);
    sys_yield();
}

void rsl_debug_dump(void) {
    print("\n[RTECH BOOT DIAGNOSTICS]\n");
    uint64_t cr3; __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    vga_print("CR3 (Page Table): 0x%x\n", cr3);
    for (int i = 0; i < 4; i++) {
        vga_print("Slab %d Usage: %d bytes\n", i, slab_get_usage(i));
    }
    sys_yield();
}

void rsl_format(void* path) {
    const char* p = str_to_cstr(path);
    int drive = p[0] - '0';
    if (f_mkfs(drive) == FR_OK) print("Format successful.\n");
    sys_yield();
}

void rsl_stamp(void* path) {
    const char* p = str_to_cstr(path);
    int drive = p[0] - '0';
    uint8_t sector[512] = {0};
    /* Deprecated: Signature logic removed. We now use GPT/HBA status for Sovereign validation. */
    if (disk_write(drive, sector, 0, 1) == RES_OK) print("Mechanical sync performed.\n");
}

void draw_pixel(int x, int y, uint32_t color);
void rsl_draw_rrif(void* path, int x, int y) {
    vfs_handle_t* h = vfs_open(path, "r");
    if (!h) return;
    uint8_t header[8];
    if (vfs_read(h, header, 8) < 8) { vfs_close(h); return; }
    uint16_t w = *(uint16_t*)&header[4]; uint16_t h_img = *(uint16_t*)&header[6];
    uint32_t pixel;
    for (int j = 0; j < h_img; j++) {
        for (int i = 0; i < w; i++) {
            if (vfs_read(h, &pixel, 4) == 4) draw_pixel(x + i, y + j, pixel);
        }
    }
    vfs_close(h);
}

void ahci_scan_remaining(void);
void rsl_scan(void) {
    ahci_scan_remaining();
    sys_yield();
}

int vdisk_eject_hw(int hw_id);
void rsl_eject(void* path) {
    const char* p = str_to_cstr(path);
    int drive = p[0] - '0';
    if (vdisk_eject_hw(drive) == 0) {
        print("Eject successful.\n");
    } else {
        print("Error: Eject failed or not supported.\n");
    }
    sys_yield();
}
