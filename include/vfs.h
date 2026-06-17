#ifndef VFS_H
#define VFS_H

#include <include/rsl.h>
#include <stdbool.h>

typedef struct vfs_handle_s vfs_handle_internal_t;

typedef struct {
    char name[32];
    void* private_data;
    void (*ls)(void* path, void* priv);
    void (*cat)(void* path, void* priv);
    void (*write)(void* path, void* content, void* priv);
    void (*cd)(void* path, void* priv);
    void (*mkdir)(void* path, void* priv);
    void (*rmdir)(void* path, void* priv);
    bool (*exists)(void* path, void* priv);
    vfs_handle_internal_t* (*open)(void* path, const char* mode, void* priv);
} vfs_node_t;

void vfs_init(void);
void vfs_register_node(vfs_node_t node);
void vfs_ls(void* path);
void vfs_cat(void* path);
void vfs_write_dispatch(void* path, void* content);
void vfs_cd(void* path);
void vfs_mkdir(void* path);
void vfs_rmdir(void* path);
bool vfs_exists(void* path);

/* Sovereign File Access Bridge */
struct vfs_handle_s {
    void* obj;          /* Node-specific pointer (e.g. FIL* or iso_handle_t) */
    void* private_data; /* VFS node private data */
    int (*read)(vfs_handle_internal_t* h, void* buf, int len);
    int (*write)(vfs_handle_internal_t* h, const void* buf, int len);
    void (*close)(vfs_handle_internal_t* h);
    uint32_t pos;
    uint32_t size;
    /* Legacy FATFS fields (retained for bridge compatibility) */
    uint32_t sclust;
    uint32_t clust;
    uint32_t entry_lba;
    uint32_t entry_idx;
};

bool vfs_is_safe_mode(void);
void vfs_set_safe_mode(bool active);

vfs_handle_internal_t* vfs_open(void* path, const char* mode);
int vfs_read(vfs_handle_internal_t* h, void* buf, int len);
int vfs_write(vfs_handle_internal_t* h, const void* buf, int len);
uint32_t vfs_tell(vfs_handle_internal_t* h);
void vfs_close(vfs_handle_internal_t* h);

#endif
