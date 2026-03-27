#include <include/rsl.h>
#include <include/vfs.h>
#include <kernel/libs/fatfs/ff.h>

int get_vdisk_count(void);

void rsl_ls(void* path) {
    vfs_ls(path);
}

void internal_rsl_ls(void* path) {
    const char* p = str_to_cstr(path);

    /* Global Root Case: List Disks */
    if (p[0] == '/' && p[1] == '\0') {
        int count = get_vdisk_count();
        for (int i = 0; i < count; i++) {
            char buf[8];
            buf[0] = '0' + i; buf[1] = ':'; buf[2] = '/'; buf[3] = '\n'; buf[4] = '\0';
            print(buf);
        }
        return;
    }

    /* Disk Root Case: List Partitions */
    /* Check for format "N:/" where N is a digit */
    if (p[0] >= '0' && p[0] <= '9' && p[1] == ':' && p[2] == '/' && p[3] == '\0') {
        int drive = p[0] - '0';
        if (drive < get_vdisk_count()) {
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

void rsl_mount(void* path) {
    FATFS fs;
    if (f_mount(&fs, str_to_cstr(path), 1) == FR_OK) {
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
