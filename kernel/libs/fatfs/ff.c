#include "ff.h"
#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

void vga_print(const char* fmt, ...);
void serial_write_str(const char* s);
void serial_print_hex(const char* label, uint16_t val);

bool safe_mode = true;
static FATFS* drive_table[16] = {0};

typedef struct {
    uint8_t name[11];
    uint8_t attr;
    uint8_t rsv[8];
    uint16_t first_cluster_high;
    uint16_t mod_time;
    uint16_t mod_date;
    uint16_t first_cluster_low;
    uint32_t size;
} __attribute__((packed)) fat_dir_entry_t;

FRESULT f_fdisk(int drive) {
    uint8_t mbr[512];
    disk_read(drive, mbr, 0, 1);
    uint8_t* p = &mbr[446];
    p[0] = 0x80; p[4] = 0x0C;
    *(uint32_t*)&p[8] = 2048;
    *(uint32_t*)&p[12] = 129024;
    mbr[510] = 0x55; mbr[511] = 0xAA;
    disk_write(drive, mbr, 0, 1);
    return FR_OK;
}

static int get_drive_id(const char* path) {
    int drive = 0;
    int i = 0;
    if (path[i] < '0' || path[i] > '9') return 0;
    while (path[i] >= '0' && path[i] <= '9') {
        drive = drive * 10 + (path[i++] - '0');
    }
    return drive;
}

FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt) {
    (void)opt;
    int drive = get_drive_id(path);
    if (drive < 0 || drive >= 16) return FR_INVALID_DRIVE;
    uint8_t sector[512];

    if (disk_read(drive, sector, 0, 1) != RES_OK) {
        vga_print("[FS] ERROR: Physical read failed on drive %d\n", drive);
        return FR_DISK_ERR;
    }

    uint32_t part_lba = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t* p = &sector[446 + (i * 16)];
        if (p[4] == 0x0C) {
            part_lba = *(uint32_t*)&p[8];
            break;
        }
        if (p[4] == 0xEE) {
            /* GPT Detected - Looking for partition at LBA 2048 */
            uint8_t gpt[512];
            if (disk_read(drive, gpt, 1, 1) == RES_OK) {
                if (*(uint64_t*)gpt == 0x5452415020494645ULL) { /* "EFI PART" */
                    /* Read first partition entry from LBA 2 */
                    if (disk_read(drive, gpt, 2, 1) == RES_OK) {
                        part_lba = (uint32_t)*(uint64_t*)&gpt[32];
                        vga_print("[FS] GPT Sovereign Partition detected at LBA %d.\n", part_lba);
                        break;
                    }
                }
            }
        }
    }
    if (part_lba == 0) {
        vga_print("[FS] ERROR: No valid partition (FAT32 or GPT) found.\n");
        return FR_NO_FILESYSTEM;
    }

    if (disk_read(drive, sector, part_lba, 1) != RES_OK) {
        vga_print("[FS] ERROR: Failed to read BPB at LBA %d\n", part_lba);
        return FR_DISK_ERR;
    }
    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        vga_print("[FS] ERROR: Invalid Boot Sector Signature (Expected 0xAA55)\n");
        return FR_NO_FILESYSTEM;
    }

    fs->partition_lba = part_lba;
    fs->reserved_sectors = *(uint16_t*)&sector[14];
    fs->n_fats = sector[16];
    fs->sectors_per_fat = *(uint32_t*)&sector[36];
    fs->sectors_per_cluster = sector[13];
    fs->root_cluster = *(uint32_t*)&sector[44];
    fs->sector_size = *(uint16_t*)&sector[11];
    fs->data_lba = part_lba + fs->reserved_sectors + (fs->n_fats * fs->sectors_per_fat);
    fs->drv = (uint8_t)drive;

    fs->active = true;
    safe_mode = false;
    drive_table[drive] = fs;
    serial_write_str("[FS] Mechanical FAT32 Mount Successful.\n");
    return FR_OK;
}

static uint32_t get_next_cluster(FATFS* fs, uint32_t cluster) {
    uint32_t ss = fs->sector_size ? fs->sector_size : 512;
    uint32_t fat_sector = fs->partition_lba + fs->reserved_sectors + (cluster * 4 / ss);
    uint32_t fat_offset = (cluster * 4) % ss;
    uint8_t buf[ss];
    if (disk_read(fs->drv, buf, fat_sector, 1) != RES_OK) return 0x0FFFFFFF;
    return (*(uint32_t*)&buf[fat_offset]) & 0x0FFFFFFF;
}

static void to_sfn(const char* src, uint8_t* dst) {
    for (int i = 0; i < 11; i++) dst[i] = ' ';
    int i = 0, j = 0;
    while (src[i] && src[i] != '.' && j < 8) dst[j++] = src[i++];
    while (src[i] && src[i] != '.') i++;
    if (src[i] == '.') {
        i++;
        j = 8;
        while (src[i] && j < 11) dst[j++] = src[i++];
    }
    for (int k = 0; k < 11; k++) if (dst[k] >= 'a' && dst[k] <= 'z') dst[k] -= 32;
}

static uint32_t find_entry(FATFS* fs, uint32_t dir_cluster, const char* name, fat_dir_entry_t* out_entry) {
    if (!fs->active) return 0;
    uint32_t cluster = dir_cluster;
    uint8_t sfn[11];
    to_sfn(name, sfn);

    while (cluster < 0x0FFFFFF8) {
        uint32_t lba = fs->data_lba + (cluster - 2) * fs->sectors_per_cluster;
        fat_dir_entry_t entries[16];
        for (uint8_t s = 0; s < fs->sectors_per_cluster; s++) {
            if (disk_read(fs->drv, (BYTE*)entries, lba + s, 1) != RES_OK) return 0;
            for (int i = 0; i < 16; i++) {
                if (entries[i].name[0] == 0) return 0;
                if (entries[i].name[0] == 0xE5) continue;
                bool match = true;
                for (int k = 0; k < 11; k++) if (entries[i].name[k] != sfn[k]) match = false;
                if (match) {
                    if (out_entry) *out_entry = entries[i];
                    return (uint32_t)entries[i].first_cluster_low | ((uint32_t)entries[i].first_cluster_high << 16);
                }
            }
        }
        cluster = get_next_cluster(fs, cluster);
    }
    return 0;
}

FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode) {
    int drive = get_drive_id(path);
    FATFS* fs = drive_table[drive];
    if (!fs) return FR_NOT_ENABLED;
    fp->obj = fs;
    if (safe_mode && (mode & FA_WRITE)) {
        vga_print("[FS] ERROR: Denied (Safe Mode is ACTIVE)\n");
        return FR_DENIED;
    }
    if (!fs->active) {
        vga_print("[FS] ERROR: Filesystem not mounted\n");
        return FR_DENIED;
    }

    uint32_t cluster = fs->root_cluster;
    char name[256];
    int i = 0, j = 0;

    if (path[i] >= '0' && path[i] <= '9') {
        while (path[i] >= '0' && path[i] <= '9') i++;
        if (path[i] == ':') i++;
    }
    if (path[i] == '/') i++;

    while (path[i]) {
        j = 0;
        while (path[i] && path[i] != '/') name[j++] = path[i++];
        name[j] = '\0';
        if (path[i] == '/') i++;

        fat_dir_entry_t entry;
        cluster = find_entry(fs, cluster, name, &entry);
        if (!cluster) return FR_NO_FILE;
        if (!path[i]) {
            fp->sclust = cluster;
            fp->clust = fp->sclust;
            fp->fptr = 0;
            fp->fsize = entry.size;
            return FR_OK;
        }
    }
    return FR_NO_FILE;
}

FRESULT f_read(FIL* fp, void* buff, uint32_t btr, uint32_t* br) {
    FATFS* fs = fp->obj;
    if (!fs->active) return FR_DENIED;
    uint32_t ss = fs->sector_size ? fs->sector_size : 512;
    uint32_t sector_in_cluster = (fp->fptr / ss) % fs->sectors_per_cluster;
    uint32_t lba = fs->data_lba + (fp->clust - 2) * fs->sectors_per_cluster + sector_in_cluster;

    if (disk_read(fs->drv, buff, lba, 1) == RES_OK) {
        uint32_t read = btr > ss ? ss : btr;
        if (br) *br = read;
        fp->fptr += read;
        if (fp->fptr % (ss * fs->sectors_per_cluster) == 0) {
            fp->clust = get_next_cluster(fs, fp->clust);
        }
        return FR_OK;
    }
    vga_print("[FS] READ ERROR: Drive %d, LBA %d\n", fs->drv, lba);
    return FR_DISK_ERR;
}

static uint32_t find_free_cluster(FATFS* fs);

static FRESULT set_cluster_link(FATFS* fs, uint32_t cluster, uint32_t next) {
    uint32_t ss = fs->sector_size ? fs->sector_size : 512;
    uint32_t fat_sector = fs->partition_lba + fs->reserved_sectors + (cluster * 4 / ss);
    uint8_t fat_buf[ss];
    if (disk_read(fs->drv, fat_buf, fat_sector, 1) != RES_OK) return FR_DISK_ERR;
    ((uint32_t*)fat_buf)[(cluster * 4 % ss) / 4] = next & 0x0FFFFFFF;
    if (disk_write(fs->drv, fat_buf, fat_sector, 1) != RES_OK) return FR_DISK_ERR;
    return FR_OK;
}

FRESULT f_write(FIL* fp, const void* buff, uint32_t btw, uint32_t* bw) {
    FATFS* fs = fp->obj;
    if (safe_mode || !fs->active) return FR_DENIED;
    uint32_t ss = fs->sector_size ? fs->sector_size : 512;
    uint32_t bytes_left = btw;
    const uint8_t* p = (const uint8_t*)buff;

    while (bytes_left > 0) {
        uint32_t sector_in_cluster = (fp->fptr / ss) % fs->sectors_per_cluster;
        uint32_t cluster_offset = fp->fptr % (ss * fs->sectors_per_cluster);

        /* If we are at the start of a new cluster (except the first one), allocate if necessary */
        if (fp->fptr > 0 && cluster_offset == 0) {
            uint32_t next = get_next_cluster(fs, fp->clust);
            if (next >= 0x0FFFFFF8) {
                /* Allocate new cluster */
                next = find_free_cluster(fs);
                if (!next) return FR_DENIED;
                set_cluster_link(fs, fp->clust, next);
                set_cluster_link(fs, next, 0x0FFFFFFF);
            }
            fp->clust = next;
        }

        uint32_t lba = fs->data_lba + (fp->clust - 2) * fs->sectors_per_cluster + sector_in_cluster;
        if (disk_write(fs->drv, p, lba, 1) != RES_OK) break;

        p += ss;
        fp->fptr += ss;
        if (bytes_left > ss) bytes_left -= ss; else bytes_left = 0;
    }

    if (bw) *bw = btw - bytes_left;
    /* Update file size in directory entry would happen on close */
    return FR_OK;
}

FRESULT f_opendir(DIR* dp, const TCHAR* path) {
    int drive = get_drive_id(path);
    FATFS* fs = drive_table[drive];
    if (!fs) return FR_NOT_ENABLED;
    dp->obj = fs;
    if (safe_mode || !fs->active) return FR_DENIED;
    dp->sclust = fs->root_cluster;

    int i = 0;
    if (path[i] >= '0' && path[i] <= '9') {
        while (path[i] >= '0' && path[i] <= '9') i++;
        if (path[i] == ':') i++;
    }
    if (path[i] == '/') i++;

    if (path[i]) {
        /* Basic path traversal for opendir */
        uint32_t cluster = fs->root_cluster;
        char name[256];
        while (path[i]) {
            int j = 0;
            while (path[i] && path[i] != '/') name[j++] = path[i++];
            name[j] = '\0';
            if (path[i] == '/') i++;
            cluster = find_entry(fs, cluster, name, NULL);
            if (!cluster) return FR_NO_PATH;
        }
        dp->sclust = cluster;
    }

    dp->clust = dp->sclust;
    dp->index = 0;
    return FR_OK;
}

FRESULT f_readdir(DIR* dp, FILINFO* fno) {
    FATFS* fs = dp->obj;
    if (safe_mode || !fs->active) return FR_DENIED;

    while (dp->clust < 0x0FFFFFF8) {
        uint32_t lba = fs->data_lba + (dp->clust - 2) * fs->sectors_per_cluster;
        fat_dir_entry_t entries[16];

        uint32_t sector_idx = (dp->index / 16);
        if (sector_idx >= fs->sectors_per_cluster) {
            dp->index = 0;
            dp->clust = get_next_cluster(fs, dp->clust);
            continue;
        }

        if (disk_read(fs->drv, (BYTE*)entries, lba + sector_idx, 1) != RES_OK) return FR_DISK_ERR;

        while ((dp->index % 16) < 16) {
            fat_dir_entry_t* e = &entries[dp->index % 16];
            dp->index++;
            if (e->name[0] == 0x00) return FR_NO_FILE;
            if (e->name[0] == 0xE5 || e->attr == 0x0F) continue;

            int k = 0;
            if (e->attr & AM_DIR) { fno->fname[k++] = '['; fno->fname[k++] = 'D'; fno->fname[k++] = 'I'; fno->fname[k++] = 'R'; fno->fname[k++] = ']'; fno->fname[k++] = ' '; }
            for (int i=0; i<8; i++) if(e->name[i]!=' ') fno->fname[k++] = e->name[i];
            if (!(e->attr & AM_DIR)) {
                fno->fname[k++] = '.';
                for (int i=8; i<11; i++) if(e->name[i]!=' ') fno->fname[k++] = e->name[i];
            }
            fno->fname[k] = '\0';
            fno->fattrib = e->attr;
            fno->fsize = e->size;
            return FR_OK;
        }
    }
    return FR_NO_FILE;
}

FRESULT f_mkfs(const TCHAR* path, BYTE opt, DWORD au) {
    (void)opt; (void)au;
    int drive = get_drive_id(path);
    if (drive < 0 || drive >= 16) return FR_INVALID_DRIVE;
    vga_print("[FS] Physical FAT32 Format Initiated on Drive %d...\n", drive);
    uint8_t boot[512] = {0};
    boot[0] = 0xEB; boot[1] = 0x58; boot[2] = 0x90;
    boot[11] = 0x00; boot[12] = 0x02; boot[13] = 0x08;
    boot[14] = 0x20; boot[16] = 0x02;
    *(uint32_t*)&boot[36] = 128; *(uint32_t*)&boot[44] = 2;
    boot[510] = 0x55; boot[511] = 0xAA;
    disk_write(drive, boot, 2048, 1);

    uint8_t fat[512] = {0};
    *(uint32_t*)&fat[0] = 0x0FFFFFF8; *(uint32_t*)&fat[4] = 0xFFFFFFFF; *(uint32_t*)&fat[8] = 0x0FFFFFFF;
    disk_write(drive, fat, 2048 + 32, 1);

    uint8_t zero[512] = {0};
    disk_write(drive, zero, 2048 + 32 + 256, 1);
    return FR_OK;
}

static uint32_t find_free_cluster(FATFS* fs) {
    uint32_t ss = fs->sector_size ? fs->sector_size : 512;
    uint8_t buf[ss];
    for (uint32_t s = 0; s < fs->sectors_per_fat; s++) {
        if (disk_read(fs->drv, buf, fs->partition_lba + fs->reserved_sectors + s, 1) == RES_OK) {
            uint32_t* fat = (uint32_t*)buf;
            for (uint32_t i = 0; i < ss/4; i++) {
                if ((fat[i] & 0x0FFFFFFF) == 0) return (s * (ss/4)) + i;
            }
        }
    }
    return 0;
}

static uint32_t parse_path_and_get_parent(FATFS* fs, const char* path, char* last_name) {
    uint32_t cluster = fs->root_cluster;
    char name[256];
    int i = 0, j = 0;

    if (path[i] >= '0' && path[i] <= '9') {
        while (path[i] >= '0' && path[i] <= '9') i++;
        if (path[i] == ':') i++;
    }
    if (path[i] == '/') i++;

    while (path[i]) {
        j = 0;
        while (path[i] && path[i] != '/') name[j++] = path[i++];
        name[j] = '\0';
        if (path[i] == '/') i++;

        if (!path[i]) {
            /* This is the last component */
            for(int k=0; k<j+1; k++) last_name[k] = name[k];
            return cluster;
        }

        fat_dir_entry_t entry;
        cluster = find_entry(fs, cluster, name, &entry);
        if (!cluster) return 0;
    }
    return 0;
}

FRESULT f_mkdir(const TCHAR* path) {
    if (safe_mode) { vga_print("[FS] MKDIR DENIED: Safe Mode Active.\n"); return FR_DENIED; }
    int drive = get_drive_id(path);
    if (drive < 0 || drive >= 16) return FR_INVALID_DRIVE;
    FATFS* fs = drive_table[drive];
    if (!fs) return FR_NOT_ENABLED;

    char name[256];
    uint32_t parent_cluster = parse_path_and_get_parent(fs, path, name);
    if (!parent_cluster) return FR_NO_PATH;

    /* Collision Check */
    if (find_entry(fs, parent_cluster, name, NULL) != 0) return FR_EXIST;

    /* 1. Find free cluster for new directory */
    uint32_t new_cluster = find_free_cluster(fs);
    if (!new_cluster) return FR_DENIED;

    /* 2. Write empty directory sector */
    uint32_t ss = fs->sector_size ? fs->sector_size : 512;
    uint8_t zero[ss];
    for(uint32_t i=0; i<ss; i++) zero[i] = 0;
    uint32_t lba = fs->data_lba + (new_cluster - 2) * fs->sectors_per_cluster;
    disk_write(fs->drv, zero, lba, 1);

    /* 3. Mark cluster as EOC in FAT */
    set_cluster_link(fs, new_cluster, 0x0FFFFFFF);

    /* 4. Add entry to parent */
    fat_dir_entry_t entry = {0};
    to_sfn(name, entry.name);
    entry.attr = AM_DIR;
    entry.first_cluster_low = new_cluster & 0xFFFF;
    entry.first_cluster_high = (new_cluster >> 16) & 0xFFFF;

    uint8_t dir_buf[ss];
    uint32_t parent_lba = fs->data_lba + (parent_cluster - 2) * fs->sectors_per_cluster;
    disk_read(fs->drv, dir_buf, parent_lba, 1);
    fat_dir_entry_t* entries = (fat_dir_entry_t*)dir_buf;
    for(int i=0; i<16; i++) {
        if (entries[i].name[0] == 0 || entries[i].name[0] == 0xE5) {
            entries[i] = entry;
            disk_write(fs->drv, dir_buf, parent_lba, 1);
            return FR_OK;
        }
    }
    return FR_DENIED;
}

FRESULT f_unlink(const TCHAR* path) {
    if (safe_mode) { vga_print("[FS] UNLINK DENIED: Safe Mode Active.\n"); return FR_DENIED; }
    int drive = get_drive_id(path);
    if (drive < 0 || drive >= 16) return FR_INVALID_DRIVE;
    FATFS* fs = drive_table[drive];
    if (!fs) return FR_NOT_ENABLED;

    char name[256];
    uint32_t parent_cluster = parse_path_and_get_parent(fs, path, name);
    if (!parent_cluster) return FR_NO_PATH;

    uint32_t ss = fs->sector_size ? fs->sector_size : 512;
    uint8_t dir_buf[ss];
    uint32_t parent_lba = fs->data_lba + (parent_cluster - 2) * fs->sectors_per_cluster;
    disk_read(fs->drv, dir_buf, parent_lba, 1);
    fat_dir_entry_t* entries = (fat_dir_entry_t*)dir_buf;
    uint8_t sfn[11];
    to_sfn(name, sfn);
    for(int i=0; i<16; i++) {
        bool match = true;
        for(int k=0; k<11; k++) if(entries[i].name[k] != sfn[k]) match = false;
        if (match) {
            entries[i].name[0] = 0xE5;
            disk_write(fs->drv, dir_buf, parent_lba, 1);
            return FR_OK;
        }
    }
    return FR_NO_FILE;
}
FRESULT f_stat(const TCHAR* path, FILINFO* fno) { (void)fno; if(path[0]=='/') return FR_OK; return FR_NO_PATH; }
FRESULT f_close(FIL* fp) { (void)fp; return FR_OK; }
void enter_safe_mode(void) { safe_mode = true; serial_write_str("[FS] SAFE MODE ACTIVE.\n"); }
