#include <include/vfs.h>
#include <include/rsl.h>
#include <kernel/libs/fatfs/ff.h>
#include <stddef.h>

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
    while (prefix[i]) i++;
    if (p[i] == ':') i++;
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

void vfs_write(void* path, void* content) {
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

void* bump_alloc(size_t size);

vfs_handle_t* vfs_open(void* path, const char* mode) {
    for (int i = 0; i < vfs_node_count; i++) {
        if (path_starts_with(path, vfs_registry[i].name)) {
            const char* subpath_cstr = strip_prefix(path, vfs_registry[i].name);
            FATFS* fs = (FATFS*)vfs_registry[i].private_data;
            if (!fs) return NULL;

            FIL fil;
            BYTE m = (mode[0] == 'w') ? (FA_WRITE | FA_CREATE_ALWAYS) : FA_READ;
            if (f_open(fs, &fil, subpath_cstr, m) == FR_OK) {
                vfs_handle_t* h = bump_alloc(sizeof(vfs_handle_t));
                h->obj = fs;
                h->sclust = fil.sclust;
                h->clust = fil.clust;
                h->size = fil.fsize;
                h->pos = fil.fptr;
                h->entry_lba = fil.entry_lba;
                h->entry_idx = fil.entry_idx;
                return h;
            }
        }
    }
    return NULL;
}

int vfs_read(vfs_handle_t* h, void* buf, int len) {
    FATFS* fs = (FATFS*)h->obj;
    uint32_t cluster_size = fs->sector_size * fs->sectors_per_cluster;

    /* Calculate clusters to skip */
    uint32_t clusters_to_skip = h->pos / cluster_size;
    uint32_t current_cluster = h->sclust;

    /* The "Walk": Follow the chain to the correct cluster */
    for (uint32_t i = 0; i < clusters_to_skip; i++) {
        current_cluster = f_get_next_cluster(fs, current_cluster);
        if (current_cluster >= 0x0FFFFFF8) return -1; /* EOF */
    }

    h->clust = current_cluster;

    FIL fil;
    fil.obj = fs;
    fil.sclust = h->sclust;
    fil.clust = h->clust;
    fil.fptr = h->pos;
    fil.fsize = h->size;
    fil.entry_lba = h->entry_lba;
    fil.entry_idx = h->entry_idx;

    uint32_t br;
    if (f_read(&fil, buf, (uint32_t)len, &br) == FR_OK) {
        h->pos = fil.fptr;
        h->clust = fil.clust;
        return (int)br;
    }
    return -1;
}

uint32_t vfs_tell(vfs_handle_t* h) {
    return h->pos;
}

void vfs_close(vfs_handle_t* h) {
    if (!h) return;
    FIL fil;
    fil.obj = (FATFS*)h->obj;
    fil.sclust = h->sclust;
    fil.clust = h->clust;
    fil.fptr = h->pos;
    fil.fsize = h->size;
    fil.entry_lba = h->entry_lba;
    fil.entry_idx = h->entry_idx;
    f_close(&fil);
}
