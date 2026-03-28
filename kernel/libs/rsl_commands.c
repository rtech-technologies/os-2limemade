#include <include/rsl.h>
#include <include/vfs.h>
#include <kernel/libs/fatfs/ff.h>

int get_hw_disk_count(void);
int get_connect_disk_count(void);

void rsl_ls(void* path) {
    vfs_ls(path);
}

void internal_rsl_ls(void* path) {
    const char* p = str_to_cstr(path);

    /* Global Root Case: List all physical hardware (mounted or not) */
    if (p[0] == '/' && p[1] == '\0') {
        int count = get_hw_disk_count();
        for (int i = 0; i < count; i++) {
            char buf[32];
            uint8_t sector[512];
            bool sovereign = false;
            int vdisk_read_hw(int hw_id, uint64_t lba, uint32_t count, void* buffer);
            if (vdisk_read_hw(i, 0, 1, sector) == 0) {
                if (sector[0] == 0xEF && sector[1] == 0xBE && sector[2] == 0xAD && sector[3] == 0xDE) {
                    sovereign = true;
                }
            }

            int k = 0;
            buf[k++] = '0' + i; buf[k++] = ':'; buf[k++] = '/'; buf[k++] = ' ';
            if (sovereign) {
                const char* tag = "[SOVEREIGN]";
                while(*tag) buf[k++] = *tag++;
            } else {
                const char* tag = "[RAW DISK]";
                while(*tag) buf[k++] = *tag++;
            }
            buf[k++] = '\n'; buf[k++] = '\0';
            print(buf);
        }
        return;
    }

    /* /CONNECT Case: List only mounted/connected disks */
    if (str_match(path, "/CONNECT")) {
        int count = get_connect_disk_count();
        if (count == 0) {
            print("No disks currently connected to /CONNECT.\n");
        } else {
            for (int i = 0; i < count; i++) {
                char buf[16];
                buf[0] = '0' + i; buf[1] = ':'; buf[2] = '/'; buf[3] = '\n'; buf[4] = '\0';
                print(buf);
            }
        }
        return;
    }

    /* Disk Root Case: List Partitions */
    /* Check for format "N:/" where N is a digit */
    if (p[0] >= '0' && p[0] <= '9' && p[1] == ':' && p[2] == '/' && p[3] == '\0') {
        int drive = p[0] - '0';
        if (drive < get_hw_disk_count()) {
            char buf[8];
            buf[0] = p[0]; buf[1] = ':'; buf[2] = '/'; buf[3] = '0'; buf[4] = '/'; buf[5] = '\n'; buf[6] = '\0';
            print(buf);
        } else {
            print("Error: Disk not found.\n");
        }
        return;
    }

    DIR dp;
    FILINFO fno;
    FRESULT res;

    res = f_opendir(&dp, p);
    if (res == FR_OK) {
        while (1) {
            res = f_readdir(&dp, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break;
            print(fno.fname);
            print("\n");
        }
    } else {
        print("Error: Could not open directory.\n");
    }
}

void rsl_cat(void* path) {
    vfs_cat(path);
}

void internal_rsl_cat(void* path) {
    FIL fp;
    FRESULT res;
    char buffer[512];
    uint32_t br;

    res = f_open(&fp, str_to_cstr(path), FA_READ);
    if (res == FR_OK) {
        while (f_read(&fp, buffer, sizeof(buffer)-1, &br) == FR_OK && br > 0) {
            buffer[br] = '\0';
            print(buffer);
        }
        f_close(&fp);
    } else {
        print("Error: Could not open file for reading.\n");
    }
}

void rsl_write(void* path, void* content) {
    vfs_write(path, content);
}

void internal_rsl_write(void* path, void* content) {
    FIL fp;
    FRESULT res;
    uint32_t bw;

    res = f_open(&fp, str_to_cstr(path), FA_WRITE | FA_CREATE_ALWAYS);
    if (res == FR_OK) {
        f_write(&fp, str_to_cstr(content), (uint32_t)str_len(content), &bw);
        f_close(&fp);
        print("Successfully wrote to disk.\n");
    } else {
        print("Error: Could not open file for writing.\n");
    }
}

void rsl_cd(void* path) {
    vfs_cd(path);
}

void internal_rsl_cd(void* path) {
    print("Changed directory context to: ");
    print(str_to_cstr(path));
    print("\n");
}

void rsl_mkdir(void* path) {
    vfs_mkdir(path);
}

void rsl_rmdir(void* path) {
    vfs_rmdir(path);
}

bool rsl_exists(void* path) {
    return vfs_exists(path);
}

void internal_rsl_mkdir(void* path) {
    if (f_mkdir(str_to_cstr(path)) == FR_OK) {
        print("Directory created.\n");
    } else {
        print("Error: Could not create directory.\n");
    }
}

void internal_rsl_rmdir(void* path) {
    if (f_unlink(str_to_cstr(path)) == FR_OK) {
        print("Directory removed.\n");
    } else {
        print("Error: Could not remove directory.\n");
    }
}

bool internal_rsl_exists(void* path) {
    FILINFO fno;
    return f_stat(str_to_cstr(path), &fno) == FR_OK;
}

void vdisk_connect(int hw_id);

void rsl_mount(void* path) {
    FATFS fs;
    const char* p = str_to_cstr(path);
    if (f_mount(&fs, p, 1) == FR_OK) {
        int drive = p[0] - '0';
        vdisk_connect(drive);
        print("Mount successful.\n");
    } else {
        print("Error: Mount failed.\n");
    }
}

void rsl_format(void* path) {
    if (f_mkfs(str_to_cstr(path), 0, 0) == FR_OK) {
        print("Format successful.\n");
    } else {
        print("Error: Format failed.\n");
    }
}

void rsl_stamp(void* path) {
    const char* p = str_to_cstr(path);
    int drive = p[0] - '0';
    uint8_t sector[512] = {0};
    sector[0] = 0xEF; sector[1] = 0xBE; sector[2] = 0xAD; sector[3] = 0xDE;
    if (vdisk_write(drive, 0, 1, sector) == 0) {
        print("Sovereign Stamp applied to LBA 0.\n");
    } else {
        print("Error: Stamp failed.\n");
    }
}
