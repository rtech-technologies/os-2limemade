#ifndef VFS_H
#define VFS_H

#include <include/rsl.h>
#include <stdbool.h>

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
} vfs_node_t;

void vfs_init(void);
void vfs_register_node(vfs_node_t node);
void vfs_ls(void* path);
void vfs_cat(void* path);
void vfs_write(void* path, void* content);
void vfs_cd(void* path);
void vfs_mkdir(void* path);
void vfs_rmdir(void* path);
bool vfs_exists(void* path);

/* Sovereign File Access Bridge */
typedef struct {
    void* obj; /* FATFS* */
    uint32_t sclust;
    uint32_t clust;
    uint32_t size;
    uint32_t pos;
    uint32_t entry_lba;
    uint32_t entry_idx;
} vfs_handle_t;

bool vfs_is_safe_mode(void);
void vfs_set_safe_mode(bool active);

vfs_handle_t* vfs_open(void* path, const char* mode);
int vfs_read(vfs_handle_t* h, void* buf, int len);
void vfs_close(vfs_handle_t* h);

#endif
