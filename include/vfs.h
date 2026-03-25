#ifndef VFS_H
#define VFS_H

#include <include/rsl.h>
#include <stdbool.h>

typedef struct {
    char name[32];
    void (*ls)(void* path);
    void (*cat)(void* path);
    void (*write)(void* path, void* content);
    void (*cd)(void* path);
} vfs_node_t;

void vfs_init(void);
void vfs_register_node(vfs_node_t node);
void vfs_ls(void* path);
void vfs_cat(void* path);
void vfs_write(void* path, void* content);
void vfs_cd(void* path);

#endif
