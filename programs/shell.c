#include <include/rsl.h>

/* Standalone Shell doesn't have access to kernel-internal vfs_handle_t directly,
   but we use the RSL syscalls via libc wrappers. */
typedef void* vfs_handle_t;

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

vfs_handle_t open_file(const char* path, const char* mode) {
    vfs_handle_t h = NULL;
    __asm__ volatile ("int $3" : : "a"((uint64_t)122), "b"((uint64_t)path), "c"((uint64_t)mode), "d"((uint64_t)&h) : "memory");
    return h;
}

int read_file(vfs_handle_t h, void* buf, int len) {
    volatile int br = -1;
    __asm__ volatile ("int $3" : : "a"((uint64_t)123), "b"((uint64_t)h), "c"((uint64_t)buf), "d"((uint64_t)&br), "S"((uint64_t)len) : "memory");
    return br;
}

void close_file(vfs_handle_t h) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)124), "b"((uint64_t)h) : "memory");
}

void set_uid(uint32_t uid) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)110), "b"((uint64_t)uid) : "memory");
}

#define COLOR_BG      0x1A1B26
#define COLOR_PANEL   0x24283B
#define COLOR_TEXT    0xC0CAF5
#define COLOR_ACCENT  0x7AA2F7
#define COLOR_SUCCESS 0x9ECE6A
#define COLOR_ERROR   0xF7768E

void draw_login_screen(rsl_fb_t* fb) {
    gui_draw_rect(fb, 0, 0, fb->width, fb->height, COLOR_BG);

    int pw = 400;
    int ph = 250;
    int px = (fb->width - pw) / 2;
    int py = (fb->height - ph) / 2;

    gui_draw_rect(fb, px, py, pw, ph, COLOR_PANEL);
    gui_draw_rect(fb, px, py, pw, 2, COLOR_ACCENT);

    gui_draw_text(fb, px + 20, py + 20, "SOVEREIGN OS v2.0", COLOR_ACCENT);
    gui_draw_text(fb, px + 20, py + 40, "Mechanical Truth Protocol Active", COLOR_TEXT);
}

void _start(void) {
    rsl_fb_t fb;
    bool has_gui = (rsl_get_fb(&fb) == 0);

    if (has_gui) {
        draw_login_screen(&fb);
    }

    set_color(CYAN, BLACK);
    print("\n[ OSx2 SOVEREIGN LOGIN ]\n");

    while (1) {
        void* user_input = input("Username: ");
        if (!user_input || str_is_empty(user_input)) {
            if (user_input) release(user_input);
            continue;
        }

        const char* uname = str_to_cstr(user_input);

        if (cstr_match(uname, "root") || cstr_match(uname, "system")) {
             print("Access Granted. System Master active.\n");
             set_uid(0);
             release(user_input);
             break;
        }

        /* Real OS Login Handshake: JSONL Audit */
        char jsonl_path[128] = "BOOT:/sys/users.jsonl";
        if (rsl_exists(str_create(jsonl_path))) {
            void* pass_input = input("Password: ");
            if (pass_input) {
                const char* pass = str_to_cstr(pass_input);
                vfs_handle_t h = open_file(jsonl_path, "r");
                if (h) {
                    char buffer[4096];
                    int br = read_file(h, buffer, 4095);
                    buffer[br] = '\0';
                    close_file(h);

                    /* Robust JSONL scan: Ensure "user":"uname" followed by "," or "}" */
                    char user_key[128] = "\"user\":\"";
                    int uk = 8; int rk = 0; while(uname[rk]) user_key[uk++] = uname[rk++];
                    user_key[uk++] = '\"'; user_key[uk] = '\0';

                    char pass_key[128] = "\"pass\":\"";
                    int pk = 8; rk = 0; while(pass[rk]) pass_key[pk++] = pass[rk++];
                    pass_key[pk++] = '\"'; pass_key[pk] = '\0';

                    bool found = false;
                    char* line = buffer;
                    while (line && *line) {
                        char* next_line = strstr(line, "\n");
                        if (next_line) *next_line = '\0';

                        if (strstr(line, user_key) && strstr(line, pass_key)) {
                            /* Basic delimiter check to prevent substring user match */
                            char* p = strstr(line, user_key);
                            if (p && (p[uk] == ',' || p[uk] == '}')) {
                                found = true;
                                break;
                            }
                        }

                        if (!next_line) break;
                        line = next_line + 1;
                    }

                    if (found) {
                        set_color(GREEN, BLACK);
                        print("Access Granted. Welcome, "); print(uname); print(".\n");
                        set_uid(1000);
                        release(pass_input);
                        release(user_input);
                        break;
                    }
                }
                set_color(RED, BLACK);
                print("Mechanical Error: Credentials rejected.\n");
                set_color(CYAN, BLACK);
                release(pass_input);
                release(user_input);
                continue;
            }
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

                vfs_handle_t h = open_file(inf_path, "r");
                if (h) {
                    char file_data[256];
                    int br = read_file(h, file_data, 255);
                    file_data[br] = '\0';
                    close_file(h);

                    char* hash_str = file_data;
                    while (*hash_str && *hash_str != '\n') hash_str++;
                    if (*hash_str == '\n') {
                        hash_str++;
                        uint64_t stored_hash = 0;
                        for (int i = 0; i < 16; i++) {
                            int val = 0;
                            if (hash_str[i] >= '0' && hash_str[i] <= '9') val = hash_str[i] - '0';
                            else if (hash_str[i] >= 'A' && hash_str[i] <= 'F') val = hash_str[i] - 'A' + 10;
                            else if (hash_str[i] >= 'a' && hash_str[i] <= 'f') val = hash_str[i] - 'a' + 10;
                            stored_hash = (stored_hash << 4) | (val & 0xF);
                        }

                        if (entered_hash == stored_hash) {
                            set_color(GREEN, BLACK);
                            print("Access Granted. Welcome, "); print(uname); print(".\n");
                            set_uid(1000);
                            release(pass_input);
                            release(user_input);
                            break;
                        }
                    }
                }
                set_color(RED, BLACK);
                print("Mechanical Error: Credentials rejected.\n");
                set_color(CYAN, BLACK);
                release(pass_input);
            }
        } else {
            set_color(RED, BLACK);
            print("Mechanical Error: Property owner not found.\n");
            set_color(CYAN, BLACK);
        }
        release(user_input);
    }

    if (has_gui) {
        gui_draw_rect(&fb, 0, 0, fb.width, fb.height, 0x000000);
    }

    set_color(GREEN, BLACK);
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
