#include <include/rsl.h>
#include <include/vfs.h>
#include <kernel/libs/fatfs/ff.h>

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

void rsl_ls(void* path) { vfs_ls(path); }
void rsl_cat(void* path) { vfs_cat(path); }
void rsl_write(void* path, void* content) { vfs_write(path, content); }
void rsl_cd(void* path) { vfs_cd(path); }
void rsl_mkdir(void* path) { vfs_mkdir(path); }
void rsl_rmdir(void* path) { vfs_rmdir(path); }
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
}

void rsl_format(void* path) {
    const char* p = str_to_cstr(path);
    int drive = p[0] - '0';
    if (f_mkfs(drive) == FR_OK) print("Format successful.\n");
}

void rsl_stamp(void* path) {
    const char* p = str_to_cstr(path);
    int drive = p[0] - '0';
    uint8_t sector[512] = {0};
    sector[0] = 0xEF; sector[1] = 0xBE; sector[2] = 0xAD; sector[3] = 0xDE;
    if (disk_write(drive, sector, 0, 1) == RES_OK) print("Sovereign Stamp applied.\n");
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
}
