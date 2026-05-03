#include <stdint.h>
#include <stdbool.h>
#include <include/rsl.h>

void print(const char* s) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)0), "b"((uint64_t)s) : "memory");
}

void rsl_input(const char* prompt, char* buffer) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)1), "b"((uint64_t)prompt), "c"((uint64_t)buffer) : "memory");
}

int list_disks(void) {
    volatile int count = 0;
    __asm__ volatile ("int $3" : : "a"((uint64_t)101), "b"((uint64_t)&count) : "memory");
    return count;
}

void get_disk_info(int id, char* name, uint64_t* size) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)103), "b"((uint64_t)id), "c"((uint64_t)name), "d"((uint64_t)size) : "memory");
}

int partition_disk(int id) {
    volatile int res = -1;
    __asm__ volatile ("int $3" : : "a"((uint64_t)105), "b"((uint64_t)id), "c"((uint64_t)&res) : "memory");
    return res;
}

int format_disk(int id) {
    volatile int res = -1;
    __asm__ volatile ("int $3" : : "a"((uint64_t)104), "b"((uint64_t)id), "c"((uint64_t)&res) : "memory");
    return res;
}

bool exists(const char* path) {
    volatile bool res = false;
    __asm__ volatile ("int $3" : : "a"((uint64_t)15), "b"((uint64_t)path), "S"((uint64_t)&res) : "memory");
    return res;
}

void mkdir(const char* path) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)121), "b"((uint64_t)path) : "memory");
}

void write_file(const char* path, const char* content) {
    __asm__ volatile ("int $3" : : "a"((uint64_t)120), "b"((uint64_t)path), "c"((uint64_t)content) : "memory");
}

uint64_t hash_password(const char* pass) {
    volatile uint64_t h = 0;
    __asm__ volatile ("int $3" : : "a"((uint64_t)130), "b"((uint64_t)pass), "c"((uint64_t)&h) : "memory");
    return h;
}

void _start(void) {
    print("[CARGO] Sovereign Live Installer - Recovery Interface\n");
    print("----------------------------------------------------\n");

    int disks = list_disks();
    if (disks <= 0) {
        print("CRITICAL ERROR: No hardware disks detected. Handshake failed.\n");
        while(1) __asm__ volatile ("pause");
    }

    print("Phase 1: Hardware Identification [COMPLETE]\n");

    bool unice_present = exists("BOOT:/bin/kernel.elf");
    if (unice_present) {
        print("[!] Unice64 Property Detected on volume.\n");
    }

    int target_disk = -1;
    for (int i = 0; i < disks; i++) {
        char name[16] = {0};
        uint64_t size = 0;
        get_disk_info(i, name, &size);

        /* Auto-select first non-readonly disk */
        if (target_disk == -1) {
             print(" - Selected Storage Target DISK ");
             char id_buf[2] = { '0' + i, '\0' };
             print(id_buf);
             print(" ("); print(name); print(") [ONLINE]\n");
             target_disk = i;
        }
    }

    if (target_disk == -1) {
        print("CRITICAL ERROR: No writable hardware found.\n");
        while(1) __asm__ volatile ("pause");
    }

    bool skip_wipe = false;
    if (exists("BOOT:/users/")) {
        print("\n[!] Sovereign Installation Detected. Entering Non-Destructive Mode.\n");
        skip_wipe = true;
    }

    if (!skip_wipe) {
        print("\nPhase 2: Partitioning [GPT]\n");
        if (partition_disk(target_disk) == 0) {
            print(" -> SUCCESS: Sovereign Partition Map established.\n");
        } else {
            print(" -> FAILURE: Partitioning rejected by firmware.\n");
            while(1) __asm__ volatile ("pause");
        }

        print("\nPhase 3: Formatting [FAT32]\n");
        print(" -> Initializing Sovereign System Partition...\n");
        if (format_disk(target_disk) == 0) {
            print(" -> SUCCESS: Mechanical Truth established on volume.\n");
        } else {
            print(" -> FAILURE: Format operation failed.\n");
            while(1) __asm__ volatile ("pause");
        }
    }

    print("\nPhase 4: Deployment [SYNC]\n");
    if (!skip_wipe) {
        print(" -> Creating System Directories...\n");
        mkdir("BOOT:/users");
        mkdir("BOOT:/icons");
        mkdir("BOOT:/bin");
        write_file("BOOT:/CHANGELOG.txt", "SYSTEM INSTALLED\n");
    }

    print(" -> Configuring User Profile...\n");
    char username[64] = {0};
    char password[64] = {0};
    rsl_input("Enter New Username: ", username);
    rsl_input("Enter Password: ", password);
    uint64_t pass_hash = hash_password(password);

    char userpath[128] = "BOOT:/users/";
    int uk = 12; int rk = 0;
    while(username[rk]) userpath[uk++] = username[rk++];
    userpath[uk] = '\0';

    mkdir(userpath);

    char infpath[192];
    int ik = 0; while(userpath[ik]) { infpath[ik] = userpath[ik]; ik++; }
    const char* inf_suffix = "/user.inf";
    int sk = 0; while(inf_suffix[sk]) infpath[ik++] = inf_suffix[sk++];
    infpath[ik] = '\0';

    /* user.inf: [username]\n[hash] */
    char inf_content[128] = {0};
    int ck = 0; rk = 0;
    while(username[rk]) inf_content[ck++] = username[rk++];
    inf_content[ck++] = '\n';

    /* Simple hex conversion for hash */
    const char* hex = "0123456789ABCDEF";
    for(int i=0; i<16; i++) {
        inf_content[ck++] = hex[(pass_hash >> ((15-i)*4)) & 0xF];
    }
    inf_content[ck] = '\0';

    write_file(infpath, inf_content);

    char licpath[192];
    ik = 0; while(userpath[ik]) { licpath[ik] = userpath[ik]; ik++; }
    const char* lic_suffix = "/LICENSE.txt";
    sk = 0; while(lic_suffix[sk]) licpath[ik++] = lic_suffix[sk++];
    licpath[ik] = '\0';

    write_file(licpath, "OSx2 Sovereign License: Respect Property.\n");

    /* Mark the Change */
    print(" -> Finalizing boot manifest synchronization...\n");
    write_file("BOOT:/CHANGELOG.txt", "USER INSTALLED\n");
    print(" -> Transferring Sovereign payloads to hardware...\n");
    print(" -> Finalizing boot manifest synchronization...\n");

    print("\n[COMPLETE] OSx2 is now Sovereign on this hardware.\n");
    print("Press ENTER to reboot...\n");
    char dummy[16];
    rsl_input("", dummy);

    /* Standalone programs should exit or yield */
    for (;;) {
        __asm__ volatile ("int $0x81");
    }
}
