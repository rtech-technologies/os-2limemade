#include <include/vfs.h>
#include <include/rsl.h>
#include <kernel/libs/storage/fatfs/ff.h>
#include <kernel/libs/storage/vdisk.h>
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
    while (prefix[i] && p[i] == prefix[i]) i++;
    if (p[i] == ':') i++;
    /* Skip leading slash after prefix if present (e.g. BOOT:/file -> /file) */
    if (p[i] == '/') return &p[i];
    if (p[i] == '\0') return "/";
    return &p[i];
}

static int get_boot_drive_id(void) {
    /* Sovereign primary drive logic: SATA_HDD is 0 if registered */
    return 0;
}

static bool str_match_prefix(const char* s1, const char* prefix) {
    int i = 0;
    while (prefix[i]) {
        if (s1[i] != prefix[i]) return false;
        i++;
    }
    return true;
}

static int get_drive_id_from_path(const char* p) {
    if (*p == '/') p++;
    if (str_match_prefix(p, "BOOT:/")) return get_boot_drive_id();
    if (str_match_prefix(p, "DISK")) {
        if (p[4] >= '0' && p[4] <= '9' && p[5] == ':' && p[6] == '/') return p[4] - '0';
    }
    if (p[0] >= '0' && p[0] <= '9' && p[1] == ':' && p[2] == '/') return p[0] - '0';
    return -1;
}

static const char* get_subpath_from_path(const char* p) {
    const char* start = p;
    if (*p == '/') p++;
    if (str_match_prefix(p, "BOOT:/")) return &p[6];
    if (str_match_prefix(p, "DISK")) {
        if (p[4] >= '0' && p[4] <= '9' && p[5] == ':' && p[6] == '/') return &p[7];
    }
    if (p[0] >= '0' && p[0] <= '9' && p[1] == ':' && p[2] == '/') return &p[3];
    return start;
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

    /* Sovereign Alias Router for LS */
    int drive = get_drive_id_from_path(p);
    if (drive != -1) {
        static FATFS hardware_fs[16];
        if (drive < 16) {
            FATFS* fs = &hardware_fs[drive];
            if (!fs->active) f_mount(fs, drive);
            if (fs->active) {
                void internal_fs_ls(void* path, void* priv);
                void* subpath = str_create(get_subpath_from_path(p));
                internal_fs_ls(subpath, fs);
                release(subpath);
                return;
            }
        }
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
    const char* p = str_to_cstr(path);
    int drive = get_drive_id_from_path(p);
    if (drive != -1) {
        static FATFS hardware_fs[16];
        if (drive < 16) {
            FATFS* fs = &hardware_fs[drive];
            if (!fs->active) f_mount(fs, drive);
            if (fs->active) {
                void internal_fs_cat(void* path, void* priv);
                void* subpath = str_create(get_subpath_from_path(p));
                internal_fs_cat(subpath, fs);
                release(subpath);
                return;
            }
        }
    }

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
    const char* p = str_to_cstr(path);
    int drive = get_drive_id_from_path(p);
    if (drive != -1) {
        static FATFS hardware_fs[16];
        if (drive < 16) {
            FATFS* fs = &hardware_fs[drive];
            if (!fs->active) f_mount(fs, drive);
            if (fs->active) {
                void internal_fs_write(void* path, void* content, void* priv);
                void* subpath = str_create(get_subpath_from_path(p));
                internal_fs_write(subpath, content, fs);
                release(subpath);
                return;
            }
        }
    }

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
    const char* p = str_to_cstr(path);
    int drive = get_drive_id_from_path(p);
    if (drive != -1) {
        static FATFS hardware_fs[16];
        if (drive < 16) {
            FATFS* fs = &hardware_fs[drive];
            if (!fs->active) f_mount(fs, drive);
            if (fs->active) {
                bool internal_fs_exists(void* path, void* priv);
                void* subpath = str_create(get_subpath_from_path(p));
                bool res = internal_fs_exists(subpath, fs);
                release(subpath);
                return res;
            }
        }
    }

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
void vga_print(const char* fmt, ...);

int vfs_mount_auto(int drive_id, const char* mount_point) {
    uint8_t* sector = bump_alloc(2048);
    if (!sector) return -1;

    /* Check A: ISO 9660 (via xorriso) */
    if (vdisk_read_hw(drive_id, 16, 1, sector) == 0) {
        if (sector[1] == 'C' && sector[2] == 'D' && sector[3] == '0' && sector[4] == '0' && sector[5] == '1') {
            void internal_fs_ls(void* path, void* priv);
            void internal_fs_cat(void* path, void* priv);

            vfs_node_t node = {0};
            node.private_data = (void*)(uint64_t)drive_id;
            node.ls = internal_fs_ls;
            node.cat = internal_fs_cat;

            /* Explicit Null-Termination: Avoid Ghost Mount labels */
            int k = 0;
            while(mount_point[k] && k < 31) {
                node.name[k] = mount_point[k];
                k++;
            }
            node.name[k] = '\0';

            vfs_register_node(node);
            vga_print("[VFS] Mechanical Judge: ISO 9660 Registered at %s:/ (Drive %d)\n", node.name, drive_id);
            return 0;
        }
    }

    /* Check B: FAT32 (Sovereign Target) */
    if (vdisk_read_hw(drive_id, 0, 1, sector) == 0) {
        if (sector[82] == 'F' && sector[83] == 'A' && sector[84] == 'T' && sector[85] == '3' && sector[86] == '2') {
            vga_print("[VFS] Mechanical Judge: FAT32 Detected on Drive %d.\n", drive_id);
            return 0;
        }
    }

    return -1;
}

vfs_handle_t* vfs_open(void* path, const char* mode) {
    const char* p = str_to_cstr(path);
    int drive = -1;
    const char* subpath_cstr = p;

    /* Sovereign Alias Router: Handle BOOT:/, DISKx:/, and x:/ */
    const char* router_p = p;
    if (*router_p == '/') router_p++;

    if (str_match_prefix(router_p, "BOOT:/")) {
        drive = get_boot_drive_id();
        subpath_cstr = &router_p[6];
    } else if (str_match_prefix(router_p, "DISK")) {
        /* DISKx:/ check */
        if (router_p[4] >= '0' && router_p[4] <= '9' && router_p[5] == ':' && router_p[6] == '/') {
            drive = router_p[4] - '0';
            subpath_cstr = &router_p[7];
        }
    } else if (router_p[0] >= '0' && router_p[0] <= '9' && router_p[1] == ':' && router_p[2] == '/') {
        drive = router_p[0] - '0';
        subpath_cstr = &router_p[3];
    }

    FATFS* fs = NULL;
    if (drive != -1) {
        /* Direct Hardware Mapping */
        static FATFS hardware_fs[16];
        if (drive >= 16) return NULL;
        fs = &hardware_fs[drive];
        if (!fs->active) {
            /* Force Partition 0 for ISO (Drive 1 in VMware usually) */
            if (vdisk_is_atapi(drive)) {
                /* ATAPI ISOs are treated as raw volumes by our mounter */
            }
            if (f_mount(fs, drive) != FR_OK) return NULL;
        }
    } else {
        /* VFS Node Dispatch (BOOT, INITRD, etc) */
        for (int i = 0; i < vfs_node_count; i++) {
            if (path_starts_with(path, vfs_registry[i].name)) {
                subpath_cstr = strip_prefix(path, vfs_registry[i].name);
                fs = (FATFS*)vfs_registry[i].private_data;
                break;
            }
        }
    }

    if (!fs) return NULL;
    FIL fil;
    BYTE m = (mode[0] == 'w') ? (FA_WRITE | FA_CREATE_ALWAYS) : FA_READ;
    if (f_open(fs, &fil, subpath_cstr, m) == FR_OK) {
        vfs_handle_t* h = bump_alloc(sizeof(vfs_handle_t));
        if (!h) return NULL;
        h->obj = fs;
        h->sclust = fil.sclust;
        h->clust = fil.clust;
        h->size = fil.fsize;
        h->pos = fil.fptr;
        h->entry_lba = fil.entry_lba;
        h->entry_idx = fil.entry_idx;
        return h;
    }
    return NULL;
}

int vfs_read(vfs_handle_t* h, void* buf, int len) {
    if (!h || !h->obj) return -1;
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

int vfs_write(vfs_handle_t* h, const void* buf, int len) {
    if (!h || !h->obj) return -1;
    FATFS* fs = (FATFS*)h->obj;
    if (fs->ro) return -1;

    FIL fil;
    fil.obj = fs;
    fil.sclust = h->sclust;
    fil.clust = h->clust;
    fil.fptr = h->pos;
    fil.fsize = h->size;
    fil.entry_lba = h->entry_lba;
    fil.entry_idx = h->entry_idx;

    uint32_t bw;
    if (f_write(&fil, buf, (uint32_t)len, &bw) == FR_OK) {
        h->pos = fil.fptr;
        h->clust = fil.clust;
        h->size = fil.fsize;
        return (int)bw;
    }
    return -1;
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
    /* release(h); // Handled by ARC if caller calls release */
}
