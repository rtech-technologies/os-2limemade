#include <include/vfs.h>
#include <include/rsl.h>
#include <include/string.h>
#include <kernel/libs/storage/fatfs/ff.h>
#include <stdint.h>
#include <stddef.h>

void* arc_alloc(size_t size);
void release(void* ptr);

int fatfs_read(vfs_handle_internal_t* h, void* buf, int len) {
    FIL* fil = (FIL*)h->obj;
    uint32_t br;
    if (f_read(fil, buf, (uint32_t)len, &br) == FR_OK) {
        h->pos = fil->fptr;
        return (int)br;
    }
    return -1;
}

int fatfs_write(vfs_handle_internal_t* h, const void* buf, int len) {
    FIL* fil = (FIL*)h->obj;
    uint32_t bw;
    if (f_write(fil, buf, (uint32_t)len, &bw) == FR_OK) {
        h->pos = fil->fptr;
        h->size = fil->fsize;
        return (int)bw;
    }
    return -1;
}

void fatfs_close(vfs_handle_internal_t* h) {
    FIL* fil = (FIL*)h->obj;
    f_close(fil);
    release(fil);
}

vfs_handle_internal_t* internal_fs_open(void* path, const char* mode, void* priv) {
    FATFS* fs = (FATFS*)priv;
    const char* p = str_to_cstr(path);
    FIL* fil = arc_alloc(sizeof(FIL));
    uint8_t m = (mode[0] == 'w') ? (FA_WRITE | FA_CREATE_ALWAYS) : FA_READ;

    if (f_open(fs, fil, p, m) == FR_OK) {
        vfs_handle_internal_t* h = arc_alloc(sizeof(vfs_handle_internal_t));
        h->obj = fil;
        h->read = fatfs_read;
        h->write = fatfs_write;
        h->close = fatfs_close;
        h->pos = fil->fptr;
        h->size = fil->fsize;
        return h;
    }
    release(fil);
    return NULL;
}

void internal_fs_ls(void* path, void* priv) {
    FATFS* fs = (FATFS*)priv;
    const char* p = str_to_cstr(path);
    DIR dir;
    FILINFO fno;
    if (f_opendir(fs, &dir, p) == FR_OK) {
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
            extern void vga_print(const char* fmt, ...);
            vga_print(" %s %s\n", (fno.fattrib & AM_DIR) ? "<DIR>" : "     ", fno.fname);
        }
    }
}

void internal_fs_cat(void* path, void* priv) {
    FATFS* fs = (FATFS*)priv;
    const char* p = str_to_cstr(path);
    FIL fil;
    if (f_open(fs, &fil, p, FA_READ) == FR_OK) {
        char buf[512];
        uint32_t br;
        while (f_read(&fil, buf, 511, &br) == FR_OK && br > 0) {
            buf[br] = '\0';
            print(buf);
        }
        f_close(&fil);
    }
}

bool internal_fs_exists(void* path, void* priv) {
    FATFS* fs = (FATFS*)priv;
    const char* p = str_to_cstr(path);
    FILINFO fno;
    return f_stat(fs, p, &fno) == FR_OK;
}

void internal_fs_write(void* path, void* content, void* priv) {
    FATFS* fs = (FATFS*)priv;
    const char* p = str_to_cstr(path);
    const char* c = str_to_cstr(content);
    FIL fil;
    if (f_open(fs, &fil, p, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        uint32_t bw;
        f_write(&fil, c, (uint32_t)strlen(c), &bw);
        f_close(&fil);
    }
}

void internal_fs_mkdir(void* path, void* priv) {
    FATFS* fs = (FATFS*)priv;
    const char* p = str_to_cstr(path);
    f_mkdir(fs, p);
}

void internal_fs_rmdir(void* path, void* priv) {
    FATFS* fs = (FATFS*)priv;
    const char* p = str_to_cstr(path);
    f_unlink(fs, p);
}
