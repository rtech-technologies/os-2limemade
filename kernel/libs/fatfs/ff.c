#include "ff.h"
#include <include/rsl.h>
#include <stdint.h>
#include <stddef.h>

void vga_print(const char* fmt, ...);
void serial_write_str(const char* s);
void serial_print_hex(const char* label, uint16_t val);

/* 100% Mechanical FAT32 Context */
typedef struct {
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint32_t sectors_per_fat;
    uint8_t sectors_per_cluster;
    uint32_t root_cluster;
    uint32_t data_lba;
    uint32_t partition_lba;
    uint16_t sector_size;
    uint8_t drive;
    bool active;
} fat32_internal_t;

static fat32_internal_t fs_ctx;
bool safe_mode = true;

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
    (void)fs; (void)opt;
    int drive = get_drive_id(path);
    uint8_t sector[512];

    if (disk_read(drive, sector, 0, 1) != RES_OK) {
        vga_print("[FS] ERROR: Physical read failed on drive %d\n", drive);
        return FR_DISK_ERR;
    }
    if (*(uint32_t*)sector != 0xEFBEADDE) {
        vga_print("[FS] ERROR: Sovereign Signature Mismatch on drive %d\n", drive);
        return FR_NO_FILESYSTEM;
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

    fs_ctx.partition_lba = part_lba;
    fs_ctx.reserved_sectors = *(uint16_t*)&sector[14];
    fs_ctx.num_fats = sector[16];
    fs_ctx.sectors_per_fat = *(uint32_t*)&sector[36];
    fs_ctx.sectors_per_cluster = sector[13];
    fs_ctx.root_cluster = *(uint32_t*)&sector[44];
    fs_ctx.sector_size = *(uint16_t*)&sector[11];
    fs_ctx.data_lba = part_lba + fs_ctx.reserved_sectors + (fs_ctx.num_fats * fs_ctx.sectors_per_fat);
    fs_ctx.drive = (uint8_t)drive;

    fs_ctx.active = true;
    safe_mode = false;
    serial_write_str("[FS] Mechanical FAT32 Mount Successful.\n");
    return FR_OK;
}

static uint32_t get_next_cluster(uint32_t cluster) {
    uint32_t ss = fs_ctx.sector_size ? fs_ctx.sector_size : 512;
    uint32_t fat_sector = fs_ctx.partition_lba + fs_ctx.reserved_sectors + (cluster * 4 / ss);
    uint32_t fat_offset = (cluster * 4) % ss;
    uint8_t buf[ss];
    if (disk_read(fs_ctx.drive, buf, fat_sector, 1) != RES_OK) return 0x0FFFFFFF;
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

static uint32_t find_entry(uint32_t dir_cluster, const char* name, fat_dir_entry_t* out_entry) {
    if (!fs_ctx.active) return 0;
    uint32_t cluster = dir_cluster;
    uint8_t sfn[11];
    to_sfn(name, sfn);

    while (cluster < 0x0FFFFFF8) {
        uint32_t lba = fs_ctx.data_lba + (cluster - 2) * fs_ctx.sectors_per_cluster;
        fat_dir_entry_t entries[16];
        for (uint8_t s = 0; s < fs_ctx.sectors_per_cluster; s++) {
            if (disk_read(fs_ctx.drive, (BYTE*)entries, lba + s, 1) != RES_OK) return 0;
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
        cluster = get_next_cluster(cluster);
    }
    return 0;
}

FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode) {
    if (safe_mode && (mode & FA_WRITE)) {
        vga_print("[FS] ERROR: Denied (Safe Mode is ACTIVE)\n");
        return FR_DENIED;
    }
    if (!fs_ctx.active) {
        vga_print("[FS] ERROR: Filesystem not mounted\n");
        return FR_DENIED;
    }

    uint32_t cluster = fs_ctx.root_cluster;
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
        cluster = find_entry(cluster, name, &entry);
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
    if (!fs_ctx.active) return FR_DENIED;
    uint32_t ss = fs_ctx.sector_size ? fs_ctx.sector_size : 512;
    uint32_t sector_in_cluster = (fp->fptr / ss) % fs_ctx.sectors_per_cluster;
    uint32_t lba = fs_ctx.data_lba + (fp->clust - 2) * fs_ctx.sectors_per_cluster + sector_in_cluster;

    if (disk_read(fs_ctx.drive, buff, lba, 1) == RES_OK) {
        uint32_t read = btr > ss ? ss : btr;
        if (br) *br = read;
        fp->fptr += read;
        if (fp->fptr % (ss * fs_ctx.sectors_per_cluster) == 0) {
            fp->clust = get_next_cluster(fp->clust);
        }
        return FR_OK;
    }
    vga_print("[FS] READ ERROR: Drive %d, LBA %d\n", fs_ctx.drive, lba);
    return FR_DISK_ERR;
}

FRESULT f_write(FIL* fp, const void* buff, uint32_t btw, uint32_t* bw) {
    if (safe_mode || !fs_ctx.active) return FR_DENIED;
    uint32_t ss = fs_ctx.sector_size ? fs_ctx.sector_size : 512;
    uint32_t sector_in_cluster = (fp->fptr / ss) % fs_ctx.sectors_per_cluster;
    uint32_t lba = fs_ctx.data_lba + (fp->clust - 2) * fs_ctx.sectors_per_cluster + sector_in_cluster;

    if (disk_write(fs_ctx.drive, (BYTE*)buff, lba, 1) == RES_OK) {
        if (bw) *bw = btw;
        fp->fptr += btw;
        /* Simple write: No auto-expansion of clusters for now */
        return FR_OK;
    }
    vga_print("[FS] WRITE ERROR: Drive %d, LBA %d\n", fs_ctx.drive, lba);
    return FR_DISK_ERR;
}

FRESULT f_opendir(DIR* dp, const TCHAR* path) {
    if (safe_mode || !fs_ctx.active) return FR_DENIED;
    dp->sclust = fs_ctx.root_cluster;

    int i = 0;
    if (path[i] >= '0' && path[i] <= '9') {
        while (path[i] >= '0' && path[i] <= '9') i++;
        if (path[i] == ':') i++;
    }
    if (path[i] == '/') i++;

    if (path[i]) {
        /* Basic path traversal for opendir */
        uint32_t cluster = fs_ctx.root_cluster;
        char name[256];
        while (path[i]) {
            int j = 0;
            while (path[i] && path[i] != '/') name[j++] = path[i++];
            name[j] = '\0';
            if (path[i] == '/') i++;
            cluster = find_entry(cluster, name, NULL);
            if (!cluster) return FR_NO_PATH;
        }
        dp->sclust = cluster;
    }

    dp->clust = dp->sclust;
    dp->index = 0;
    return FR_OK;
}

FRESULT f_readdir(DIR* dp, FILINFO* fno) {
    if (safe_mode || !fs_ctx.active) return FR_DENIED;

    while (dp->clust < 0x0FFFFFF8) {
        uint32_t lba = fs_ctx.data_lba + (dp->clust - 2) * fs_ctx.sectors_per_cluster;
        fat_dir_entry_t entries[16];

        uint32_t sector_idx = (dp->index / 16);
        if (sector_idx >= fs_ctx.sectors_per_cluster) {
            dp->index = 0;
            dp->clust = get_next_cluster(dp->clust);
            continue;
        }

        if (disk_read(fs_ctx.drive, (BYTE*)entries, lba + sector_idx, 1) != RES_OK) return FR_DISK_ERR;

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

static uint32_t find_free_cluster(void) {
    uint32_t ss = fs_ctx.sector_size ? fs_ctx.sector_size : 512;
    uint8_t buf[ss];
    for (uint32_t s = 0; s < fs_ctx.sectors_per_fat; s++) {
        if (disk_read(fs_ctx.drive, buf, fs_ctx.partition_lba + fs_ctx.reserved_sectors + s, 1) == RES_OK) {
            uint32_t* fat = (uint32_t*)buf;
            for (uint32_t i = 0; i < ss/4; i++) {
                if ((fat[i] & 0x0FFFFFFF) == 0) return (s * (ss/4)) + i;
            }
        }
    }
    return 0;
}

static uint32_t parse_path_and_get_parent(const char* path, char* last_name) {
    uint32_t cluster = fs_ctx.root_cluster;
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
        cluster = find_entry(cluster, name, &entry);
        if (!cluster) return 0;
    }
    return 0;
}

FRESULT f_mkdir(const TCHAR* path) {
    if (safe_mode) { vga_print("[FS] MKDIR DENIED: Safe Mode Active.\n"); return FR_DENIED; }

    char name[256];
    uint32_t parent_cluster = parse_path_and_get_parent(path, name);
    if (!parent_cluster) return FR_NO_PATH;

    /* 1. Find free cluster for new directory */
    uint32_t new_cluster = find_free_cluster();
    if (!new_cluster) return FR_DENIED;

    /* 2. Write empty directory sector */
    uint32_t ss = fs_ctx.sector_size ? fs_ctx.sector_size : 512;
    uint8_t zero[ss];
    for(uint32_t i=0; i<ss; i++) zero[i] = 0;
    uint32_t lba = fs_ctx.data_lba + (new_cluster - 2) * fs_ctx.sectors_per_cluster;
    disk_write(fs_ctx.drive, zero, lba, 1);

    /* 3. Mark cluster as EOC in FAT */
    uint32_t fat_sector = fs_ctx.partition_lba + fs_ctx.reserved_sectors + (new_cluster * 4 / ss);
    uint8_t fat_buf[ss];
    disk_read(fs_ctx.drive, fat_buf, fat_sector, 1);
    ((uint32_t*)fat_buf)[(new_cluster * 4 % ss) / 4] = 0x0FFFFFFF;
    disk_write(fs_ctx.drive, fat_buf, fat_sector, 1);

    /* 4. Add entry to parent */
    fat_dir_entry_t entry = {0};
    to_sfn(name, entry.name);
    entry.attr = AM_DIR;
    entry.first_cluster_low = new_cluster & 0xFFFF;
    entry.first_cluster_high = (new_cluster >> 16) & 0xFFFF;

    uint8_t dir_buf[ss];
    uint32_t parent_lba = fs_ctx.data_lba + (parent_cluster - 2) * fs_ctx.sectors_per_cluster;
    disk_read(fs_ctx.drive, dir_buf, parent_lba, 1);
    fat_dir_entry_t* entries = (fat_dir_entry_t*)dir_buf;
    for(int i=0; i<16; i++) {
        if (entries[i].name[0] == 0 || entries[i].name[0] == 0xE5) {
            entries[i] = entry;
            disk_write(fs_ctx.drive, dir_buf, parent_lba, 1);
            return FR_OK;
        }
    }
    return FR_DENIED;
}

FRESULT f_unlink(const TCHAR* path) {
    if (safe_mode) { vga_print("[FS] UNLINK DENIED: Safe Mode Active.\n"); return FR_DENIED; }

    char name[256];
    uint32_t parent_cluster = parse_path_and_get_parent(path, name);
    if (!parent_cluster) return FR_NO_PATH;

    uint32_t ss = fs_ctx.sector_size ? fs_ctx.sector_size : 512;
    uint8_t dir_buf[ss];
    uint32_t parent_lba = fs_ctx.data_lba + (parent_cluster - 2) * fs_ctx.sectors_per_cluster;
    disk_read(fs_ctx.drive, dir_buf, parent_lba, 1);
    fat_dir_entry_t* entries = (fat_dir_entry_t*)dir_buf;
    uint8_t sfn[11];
    to_sfn(name, sfn);
    for(int i=0; i<16; i++) {
        bool match = true;
        for(int k=0; k<11; k++) if(entries[i].name[k] != sfn[k]) match = false;
        if (match) {
            entries[i].name[0] = 0xE5;
            disk_write(fs_ctx.drive, dir_buf, parent_lba, 1);
            return FR_OK;
        }
    }
    return FR_NO_FILE;
}
FRESULT f_stat(const TCHAR* path, FILINFO* fno) { (void)fno; if(path[0]=='/') return FR_OK; return FR_NO_PATH; }
FRESULT f_close(FIL* fp) { (void)fp; return FR_OK; }
void enter_safe_mode(void) { safe_mode = true; serial_write_str("[FS] SAFE MODE ACTIVE.\n"); }
