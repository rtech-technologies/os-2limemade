#include <include/vfs.h>
#include <include/rsl.h>
#include <kernel/libs/storage/fatfs/ff.h>
#include <kernel/libs/storage/vdisk.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_VFS_NODES 16
static vfs_node_t vfs_registry[MAX_VFS_NODES];
static int vfs_node_count = 0;
static bool safe_mode_active = true;

void vfs_init(void) {
    vfs_node_count = 0;
    safe_mode_active = true;
}

bool vfs_is_safe_mode(void) { return safe_mode_active; }
void vfs_set_safe_mode(bool active) { safe_mode_active = active; }

void vfs_register_node(vfs_node_t node) {
    if (vfs_node_count < MAX_VFS_NODES) {
        vfs_registry[vfs_node_count++] = node;
    }
}

static bool path_starts_with(void* path, const char* prefix) {
    const char* p = str_to_cstr(path);
    int i = 0;
    while (prefix[i]) {
        if (p[i] != prefix[i]) return false;
        i++;
    }
    return true;
}

static const char* strip_prefix(void* path, const char* prefix) {
    const char* p = str_to_cstr(path);
    int i = 0;
    while (prefix[i] && p[i] == prefix[i]) i++;
    if (p[i] == ':') i++;
    if (p[i] == '/') return &p[i];
    if (p[i] == '\0') return "/";
    return &p[i];
}

void vfs_ls(void* path) {
    const char* p = str_to_cstr(path);
    if (p[0] == '/' && p[1] == '\0') {
        for (int i = 0; i < vfs_node_count; i++) {
            print(vfs_registry[i].name);
            print(":/ (Mounted Node)\n");
        }
        return;
    }
    for (int i = 0; i < vfs_node_count; i++) {
        if (path_starts_with(path, vfs_registry[i].name)) {
            if (vfs_registry[i].ls) {
                void* subpath = str_create(strip_prefix(path, vfs_registry[i].name));
                vfs_registry[i].ls(subpath, vfs_registry[i].private_data);
                release(subpath);
            }
            return;
        }
    }
}

void vfs_cat(void* path) {
    for (int i = 0; i < vfs_node_count; i++) {
        if (path_starts_with(path, vfs_registry[i].name)) {
            if (vfs_registry[i].cat) {
                void* subpath = str_create(strip_prefix(path, vfs_registry[i].name));
                vfs_registry[i].cat(subpath, vfs_registry[i].private_data);
                release(subpath);
            }
            return;
        }
    }
}

void vfs_write_dispatch(void* path, void* content) {
    for (int i = 0; i < vfs_node_count; i++) {
        if (path_starts_with(path, vfs_registry[i].name)) {
            if (vfs_registry[i].write) {
                void* subpath = str_create(strip_prefix(path, vfs_registry[i].name));
                vfs_registry[i].write(subpath, content, vfs_registry[i].private_data);
                release(subpath);
            }
            return;
        }
    }
}

void vfs_cd(void* path) {
    for (int i = 0; i < vfs_node_count; i++) {
        if (vfs_registry[i].cd) {
            vfs_registry[i].cd(path, vfs_registry[i].private_data);
        }
    }
}

void vfs_mkdir(void* path) {
    for (int i = 0; i < vfs_node_count; i++) {
        if (path_starts_with(path, vfs_registry[i].name)) {
            if (vfs_registry[i].mkdir) {
                void* subpath = str_create(strip_prefix(path, vfs_registry[i].name));
                vfs_registry[i].mkdir(subpath, vfs_registry[i].private_data);
                release(subpath);
            }
            return;
        }
    }
}

void vfs_rmdir(void* path) {
    for (int i = 0; i < vfs_node_count; i++) {
        if (path_starts_with(path, vfs_registry[i].name)) {
            if (vfs_registry[i].rmdir) {
                void* subpath = str_create(strip_prefix(path, vfs_registry[i].name));
                vfs_registry[i].rmdir(subpath, vfs_registry[i].private_data);
                release(subpath);
            }
            return;
        }
    }
}

bool vfs_exists(void* path) {
    for (int i = 0; i < vfs_node_count; i++) {
        if (path_starts_with(path, vfs_registry[i].name)) {
            if (vfs_registry[i].exists) {
                void* subpath = str_create(strip_prefix(path, vfs_registry[i].name));
                bool res = vfs_registry[i].exists(subpath, vfs_registry[i].private_data);
                release(subpath);
                return res;
            }
        }
    }
    return false;
}

void* arc_alloc(size_t size);

vfs_handle_internal_t* vfs_open(void* path, const char* mode) {
    for (int i = 0; i < vfs_node_count; i++) {
        if (path_starts_with(path, vfs_registry[i].name)) {
            if (vfs_registry[i].open) {
                void* subpath = str_create(strip_prefix(path, vfs_registry[i].name));
                vfs_handle_internal_t* h = vfs_registry[i].open(subpath, mode, vfs_registry[i].private_data);
                release(subpath);
                if (h) h->private_data = vfs_registry[i].private_data;
                return h;
            }
        }
    }
    return NULL;
}

int vfs_read(vfs_handle_internal_t* h, void* buf, int len) {
    if (!h || !h->read) return -1;
    return h->read(h, buf, len);
}

int vfs_write(vfs_handle_internal_t* h, const void* buf, int len) {
    if (!h || !h->write) return -1;
    return h->write(h, (void*)buf, len);
}

void vfs_close(vfs_handle_internal_t* h) {
    if (!h || !h->close) return;
    h->close(h);
    void release(void* ptr);
    release(h);
}

uint32_t vfs_tell(vfs_handle_internal_t* h) {
    return h ? h->pos : 0;
}
