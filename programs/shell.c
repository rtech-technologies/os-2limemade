#include <include/rsl.h>
#include <include/vfs.h>

void rsl_dispatch_command(char* line, void** curdir_ptr, bool* is_safe_ptr);

static bool cstr_match(const char* s1, const char* s2) {
    int i = 0;
    while (s1[i] && s2[i]) {
        if (s1[i] != s2[i]) return false;
        i++;
    }
    return s1[i] == s2[i];
}

uint64_t hash_password(const char* pass) {
    volatile uint64_t h = 0;
    __asm__ volatile ("int $3" : : "a"((uint64_t)130), "b"((uint64_t)pass), "c"((uint64_t)&h) : "memory");
    return h;
}

vfs_handle_t* open_file(const char* path, const char* mode) {
    vfs_handle_t* h = NULL;
    __asm__ volatile ("int $3" : : "a"((uint64_t)122), "b"((uint64_t)path), "c"((uint64_t)mode), "d"((uint64_t)&h) : "memory");
    return h;
}

int read_file(vfs_handle_t* h, void* buf, int len) {
    volatile int br = -1;
    __asm__ volatile ("int $3" : : "a"((uint64_t)123), "b"((uint64_t)h), "c"((uint64_t)buf), "d"((uint64_t)&br), "S"((uint64_t)len) : "memory");
    return br;
}

void close_file(vfs_handle_t* h) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)124), "b"((uint64_t)h) : "memory");
}

void set_uid(uint32_t uid) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)110), "b"((uint64_t)uid) : "memory");
}

void shell_main(void) {
    set_color(GREEN, BLACK);

    /* Quartermaster: Secure Login Handshake */
    print("\n[ OSx2 SOVEREIGN LOGIN ]\n");

    while (1) {
        void* user_input = input("Username: ");
        if (!user_input || str_is_empty(user_input)) {
            if (user_input) release(user_input);
            continue;
        }

        const char* uname = str_to_cstr(user_input);

        /* Bypass for System Master */
        if (cstr_match(uname, "root") || cstr_match(uname, "system")) {
             print("Access Granted. System Master active.\n");
             set_uid(0);
             release(user_input);
             break;
        }

        char inf_path[128] = "BOOT:/users/";
        int uk = 12; int rk = 0;
        while(uname[rk]) inf_path[uk++] = uname[rk++];
        inf_path[uk] = '\0';
        const char* inf_suffix = "/user.inf";
        rk = 0; while(inf_suffix[rk]) inf_path[uk++] = inf_suffix[rk++];
        inf_path[uk] = '\0';

        if (rsl_exists(str_create(inf_path))) {
            void* pass_input = input("Password: ");
            if (pass_input) {
                uint64_t entered_hash = hash_password(str_to_cstr(pass_input));

                vfs_handle_t* h = open_file(inf_path, "r");
                if (h) {
                    char file_data[256];
                    int br = read_file(h, file_data, 255);
                    file_data[br] = '\0';
                    close_file(h);

                    /* Find start of hash in user.inf (second line) */
                    char* hash_str = file_data;
                    while (*hash_str && *hash_str != '\n') hash_str++;
                    if (*hash_str == '\n') {
                        hash_str++;
                        uint64_t stored_hash = 0;
                        for (int i = 0; i < 16; i++) {
                            int val = 0;
                            if (hash_str[i] >= '0' && hash_str[i] <= '9') val = hash_str[i] - '0';
                            else if (hash_str[i] >= 'A' && hash_str[i] <= 'F') val = hash_str[i] - 'A' + 10;
                            stored_hash = (stored_hash << 4) | (val & 0xF);
                        }

                        if (entered_hash == stored_hash) {
                            print("Access Granted. Welcome, "); print(uname); print(".\n");
                            set_uid(1000);
                            release(pass_input);
                            release(user_input);
                            break;
                        }
                    }
                }
                print("Mechanical Error: Credentials rejected.\n");
                release(pass_input);
            }
        } else {
            print("Mechanical Error: Property owner not found.\n");
        }
        release(user_input);
    }

    print("  _____ _______ ______ _____ _    _    ____   _____     ___  \n");
    print(" |  __ \\__   __|  ____/ ____| |  | |  / __ \\ / ____|   |__ \\ \n");
    print(" | |__) | | |  | |__ | |    | |__| | | |  | | (_____  __  ) |\n");
    print(" |  _  /  | |  |  __|| |    |  __  | | |  | |\\___ \\ \\/ / / / \n");
    print(" | | \\ \\  | |  | |___| |____| |  | | | |__| |____) >  < / /_ \n");
    print(" |_|  \\_\\ |_|  |______\\_____|_|  |_|  \\____/|_____/_/\\_\\____|\n");
    print("\n[ OSx2 Sovereign ] Build Success.\n");

    bool is_safe = false;
    void* curdir = str_create("/");

    while (1) {
        void* suffix = str_create("OS2>");
        void* prompt = str_concat(curdir, suffix);
        release(suffix);

        void* cmd_line = input(str_to_cstr(prompt));
        release(prompt);

        if (cmd_line == NULL) continue;
        if (str_is_empty(cmd_line)) { release(cmd_line); continue; }

        rsl_dispatch_command((char*)str_to_cstr(cmd_line), &curdir, &is_safe);
        release(cmd_line);
    }
}
