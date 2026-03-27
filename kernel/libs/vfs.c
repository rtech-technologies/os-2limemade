#include <include/vfs.h>
#include <include/rsl.h>
#include <stddef.h>

#define MAX_VFS_NODES 16
static vfs_node_t vfs_registry[MAX_VFS_NODES];
static int vfs_node_count = 0;

void vfs_init(void) {
    vfs_node_count = 0;
}

void vfs_register_node(vfs_node_t node) {
    if (vfs_node_count < MAX_VFS_NODES) {
        vfs_registry[vfs_node_count++] = node;
    }
}

void vfs_ls(void* path) {
    /* Route to all nodes; the vdisk node handles the global '/' case specifically. */
    for (int i = 0; i < vfs_node_count; i++) {
        if (vfs_registry[i].ls) vfs_registry[i].ls(path);
    }
}

void vfs_cat(void* path) {
    for (int i = 0; i < vfs_node_count; i++) {
        if (vfs_registry[i].cat) vfs_registry[i].cat(path);
    }
}

void vfs_write(void* path, void* content) {
    for (int i = 0; i < vfs_node_count; i++) {
        if (vfs_registry[i].write) vfs_registry[i].write(path, content);
    }
}

void vfs_cd(void* path) {
    for (int i = 0; i < vfs_node_count; i++) {
        if (vfs_registry[i].cd) vfs_registry[i].cd(path);
    }
}

void vfs_mkdir(void* path) {
    for (int i = 0; i < vfs_node_count; i++) {
        if (vfs_registry[i].mkdir) vfs_registry[i].mkdir(path);
    }
}

void vfs_rmdir(void* path) {
    for (int i = 0; i < vfs_node_count; i++) {
        if (vfs_registry[i].rmdir) vfs_registry[i].rmdir(path);
    }
}

bool vfs_exists(void* path) {
    for (int i = 0; i < vfs_node_count; i++) {
        if (vfs_registry[i].exists && vfs_registry[i].exists(path)) return true;
    }
    return false;
}
