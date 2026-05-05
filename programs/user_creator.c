#include <include/rsl.h>
#include <include/string.h>

typedef void* vfs_handle_t;

void set_uid(uint32_t uid) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)110), "b"((uint64_t)uid) : "memory");
}

uint32_t get_uid(void) {
    volatile uint32_t uid = 0xFFFFFFFF;
    __asm__ volatile ("int $3" : : "a"((uint64_t)111), "b"((uint64_t)&uid) : "memory");
    return uid;
}

uint64_t hash_password(const char* pass) {
    volatile uint64_t h = 0;
    __asm__ volatile ("int $3" : : "a"((uint64_t)130), "b"((uint64_t)pass), "c"((uint64_t)&h) : "memory");
    return h;
}

void mkdir(const char* path) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)121), "b"((uint64_t)path) : "memory");
}

void write_file(const char* path, const char* content) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)120), "b"((uint64_t)path), "c"((uint64_t)content) : "memory");
}

void _start(void) {
    uint32_t caller_uid = get_uid();

    if (caller_uid != 0 && caller_uid != 1) {
        print("Mechanical Error: Insufficient privileges for user creation.\n");
        return;
    }

    print("\n[ OSx2 USER CREATION UTILITY ]\n");
    void* name_input = input("New Username: ");
    if (!name_input) return;
    const char* name = str_to_cstr(name_input);

    void* pass_input = input("New Password: ");
    if (!pass_input) { release(name_input); return; }
    const char* pass = str_to_cstr(pass_input);

    void* level_input = input("User Level (1: Admin, 2: Standard, 3: Guest): ");
    if (!level_input) { release(pass_input); release(name_input); return; }
    const char* level = str_to_cstr(level_input);

    char user_dir[128] = "BOOT:/users/";
    int uk = 12; int rk = 0;
    while(name[rk]) user_dir[uk++] = name[rk++];
    user_dir[uk] = '\0';

    mkdir(user_dir);

    char inf_path[128];
    rk = 0; while(user_dir[rk]) { inf_path[rk] = user_dir[rk]; rk++; }
    const char* inf_suffix = "/user.inf";
    int ik = 0; while(inf_suffix[ik]) inf_path[rk++] = inf_suffix[ik++];
    inf_path[rk] = '\0';

    uint64_t h = hash_password(pass);
    char hstr[17];
    const char* hex = "0123456789ABCDEF";
    for(int i=0; i<16; i++) hstr[i] = hex[(h >> (60 - i*4)) & 0xF];
    hstr[16] = '\0';

    char content[256];
    int ck = 0;
    strcpy(content + ck, "LEVEL: ");
    ck += strlen("LEVEL: ");
    strcpy(content + ck, level);
    ck += strlen(level);
    content[ck++] = '\n';
    strcpy(content + ck, hstr);
    ck += strlen(hstr);
    content[ck++] = '\n';
    content[ck] = '\0';

    write_file(inf_path, content);

    print("User "); print(name); print(" created successfully at level "); print(level); print(".\n");

    release(level_input);
    release(pass_input);
    release(name_input);
}
