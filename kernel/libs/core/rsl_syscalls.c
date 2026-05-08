#include <include/rsl.h>
#include <include/vfs.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/services.h>
#include <kernel/unice64/task.h>
#include <kernel/libs/storage/fatfs/ff.h>
#include <limine.h>
#include <stdint.h>
#include <stddef.h>

extern struct limine_framebuffer_response* get_framebuffer(void);
void vga_print(const char* fmt, ...);

/* Bridge definitions from rsl_commands.c and rsl_string.c */
void set_color(color_t fg, color_t bg);
void* input(const char* prompt);
size_t str_len(void* str);

void rsl_syscall_handler(uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, uint64_t rsi) {
    task_t* current = get_current_task();
    /* Guest Isolation: UID 2000+ cannot write to FS */
    bool guest_lock = (current && current->uid >= 2000);
    void* arc_alloc(size_t size);

    switch(rax) {
        case 0: // print
            vga_print("%s", (const char*)rbx);
            sys_yield();
            break;
        case 1: { // rsl_input(prompt, buffer) - For legacy/buffer-based input
            void* res = input((const char*)rbx);
            if (res) {
                const char* cstr = str_to_cstr(res);
                char* out_buf = (char*)rcx;
                int k = 0;
                while (cstr[k]) { out_buf[k] = cstr[k]; k++; }
                out_buf[k] = '\0';
                release(res);
            }
            break;
        }
        case 2: // rsl_yield
            sys_yield();
            break;
        case 10: // set_color
            set_color((color_t)rbx, (color_t)rcx);
            break;
        case 11: { // input(prompt, out_ptr)
            void* res = input((const char*)rbx);
            *(void**)rcx = res;
            break;
        }
        case 20: // retain
            retain((void*)rbx);
            break;
        case 21: // release
            release((void*)rbx);
            break;
        case 30: { // str_create(cstr, out_ptr)
            void* res = str_create((const char*)rbx);
            *(void**)rcx = res;
            break;
        }
        case 31: { // str_concat(s1, s2, out_ptr)
            void* res = str_concat((void*)rbx, (void*)rcx);
            *(void**)rdx = res;
            release((void*)rbx);
            release((void*)rcx);
            break;
        }
        case 32: { // str_is_empty(str, out_bool_ptr)
            *(bool*)rcx = str_is_empty((void*)rbx);
            break;
        }
        case 35: { // str_to_cstr(str, out_ptr)
            *(const char**)rcx = str_to_cstr((void*)rbx);
            break;
        }
        case 50: // rsl_ls
            vfs_ls((void*)rbx);
            release((void*)rbx);
            break;
        case 51: // rsl_cat
            vfs_cat((void*)rbx);
            release((void*)rbx);
            break;
        case 52: // rsl_cd
            vfs_cd((void*)rbx);
            release((void*)rbx);
            break;
        case 53: // rsl_mkdir
            if (!guest_lock) vfs_mkdir((void*)rbx);
            release((void*)rbx);
            break;
        case 15: // rsl_exists
            *(bool*)rsi = vfs_exists((void*)rbx);
            release((void*)rbx);
            break;
        case 101: // rsl_list_disks
            *(int*)rbx = get_hw_disk_count();
            break;
        case 102: { // rsl_is_sovereign
            int drive = (int)rbx;
            uint8_t sector[512];
            bool* out = (bool*)rcx;
            *out = false;
            if (disk_read(drive, sector, 2048, 1) == RES_OK) {
                if (sector[510] == 0x55 && sector[511] == 0xAA) {
                    if (sector[3] == 'O' && sector[4] == 'S' && sector[5] == 'X') *out = true;
                }
            }
            if (!*out && disk_read(drive, sector, 2, 1) == RES_OK) {
                if (sector[56] == 'S' && sector[58] == 'o') *out = true;
            }
            break;
        }
        case 103: { // rsl_get_disk_info(disk_id, name_ptr, size_ptr)
            get_hw_disk_info((int)rbx, (char*)rcx, (uint64_t*)rdx);
            break;
        }
        case 104: { // rsl_format(disk_id)
            if (guest_lock) { *(int*)rcx = -1; break; }
            FRESULT f_mkfs(int drive);
            *(int*)rcx = (f_mkfs((int)rbx) == FR_OK) ? 0 : -1;
            break;
        }
        case 105: { // rsl_fdisk(disk_id)
            if (guest_lock) { *(int*)rcx = -1; break; }
            FRESULT f_fdisk(int drive);
            *(int*)rcx = (f_fdisk((int)rbx) == FR_OK) ? 0 : -1;
            break;
        }
        case 110: { // rsl_set_uid(uid)
            if (current && current->uid <= 1) current->uid = (uint32_t)rbx;
            break;
        }
        case 111: { // rsl_get_uid()
            if (current) *(uint32_t*)rbx = current->uid;
            break;
        }
        case 112: { // rsl_user_create(name, pass, out_res)
            const char* name = (const char*)rbx;
            const char* pass = (const char*)rcx;
            int* out_res = (int*)rdx;

            /* Check if name is 'guest' to assign guest UID */
            int uid = 1000;
            if (name[0] == 'g' && name[1] == 'u' && name[2] == 'e' && name[3] == 's' && name[4] == 't' && name[5] == '\0') {
                uid = 2000;
            }

            /* In a Sovereign system, we store user metadata in DATA:/sys/users.jsonl or BOOT:/sys/users.jsonl */
            void* dsys = str_create("DATA:/sys");
            const char* path_to_try = vfs_exists(dsys) ? "DATA:/sys/users.jsonl" : "BOOT:/sys/users.jsonl";
            release(dsys);

            void* s_path = str_create(path_to_try);
            void* h = vfs_open(s_path, "a");
            release(s_path);

            if (h) {
                char entry[512];
                /* JSONL entry: {"user": "name", "pass": "hash", "uid": 1000} */
                int k = 0;
                const char* head = "{\"user\":\"";
                while(head[k] && k < 510) { entry[k] = head[k]; k++; }
                int nk = 0; while(name[nk] && k < 510) { entry[k++] = name[nk++]; }
                const char* mid = "\",\"pass\":\"";
                int mk = 0; while(mid[mk] && k < 510) { entry[k++] = mid[mk++]; }
                int pk = 0; while(pass[pk] && k < 510) { entry[k++] = pass[pk++]; }
                const char* tail_head = "\",\"uid\":";
                int thk = 0; while(tail_head[thk] && k < 510) { entry[k++] = tail_head[thk++]; }

                // Manual int to string for UID
                if (uid == 2000 && k < 508) { entry[k++] = '2'; entry[k++] = '0'; entry[k++] = '0'; entry[k++] = '0'; }
                else if (k < 508) { entry[k++] = '1'; entry[k++] = '0'; entry[k++] = '0'; entry[k++] = '0'; }

                if (k < 510) { entry[k++] = '}'; entry[k++] = '\n'; entry[k] = '\0'; }
                vfs_write(h, entry, k);
                vfs_close(h);
                release(h);
                if (out_res) *out_res = 0;
            } else {
                if (out_res) *out_res = -1;
            }
            break;
        }
        case 120: { // rsl_write(path, content)
            if (!guest_lock) vfs_write_dispatch((void*)rbx, (void*)rcx);
            release((void*)rbx);
            release((void*)rcx);
            break;
        }
        case 121: { // rsl_mkdir(path)
            if (!guest_lock) vfs_mkdir((void*)rbx);
            release((void*)rbx);
            break;
        }
        case 122: { // rsl_open(path, mode)
            /* mode FA_WRITE check */
            const char* m = (const char*)rcx;
            if (guest_lock && m[0] == 'w') {
                release((void*)rbx);
                *(vfs_handle_internal_t**)rdx = NULL;
                break;
            }
            *(vfs_handle_internal_t**)rdx = vfs_open((void*)rbx, (const char*)rcx);
            release((void*)rbx);
            break;
        }
        case 123: { // rsl_read(handle, buf, len)
            *(int*)rdx = vfs_read((vfs_handle_internal_t*)rbx, (void*)rcx, (int)rsi);
            break;
        }
        case 124: { // rsl_close(handle)
            vfs_close((vfs_handle_internal_t*)rbx);
            release((void*)rbx);
            break;
        }
        case 130: { // rsl_hash(string, out_u64)
            const char* s = (const char*)rbx;
            uint64_t hash = 5381;
            int c;
            while ((c = *s++)) hash = ((hash << 5) + hash) + c;
            *(uint64_t*)rcx = hash;
            break;
        }
        case 140: { // malloc
            void** out_ptr = (void**)rcx;
            *out_ptr = arc_alloc((size_t)rbx);
            break;
        }
        case 141: { // free
            void release(void* ptr);
            release((void*)rbx);
            break;
        }
        case 202: { // rsl_get_fb
            struct limine_framebuffer_response* resp = get_framebuffer();
            if (resp && resp->framebuffer_count > 0) {
                struct limine_framebuffer* fb = resp->framebuffers[0];
                rsl_fb_t* ufb = (rsl_fb_t*)rbx;
                ufb->address = (uint64_t)fb->address;
                ufb->width = fb->width;
                ufb->height = fb->height;
                ufb->pitch = fb->pitch;
                ufb->bpp = fb->bpp;
                *(int*)rcx = 0;
            } else {
                *(int*)rcx = -1;
            }
            break;
        }
        case 300: { // rsl_dispatch_command(line, curdir_ptr, is_safe_ptr)
            void rsl_dispatch_command(char* line, void** curdir_ptr, bool* is_safe_ptr);
            rsl_dispatch_command((char*)rbx, (void**)rcx, (bool*)rdx);
            break;
        }
        case 301: { // rsl_mount_vfs(disk_id, name)
            int drive = (int)rbx;
            const char* name = (const char*)rcx;
            void* internal_fs_ls(void* path, void* priv);
            void* internal_fs_cat(void* path, void* priv);
            void* internal_fs_write(void* path, void* content, void* priv);
            void* internal_fs_mkdir(void* path, void* priv);
            void* internal_fs_rmdir(void* path, void* priv);
            bool internal_fs_exists(void* path, void* priv);

            FATFS* fs = arc_alloc(sizeof(FATFS));
            if (f_mount(fs, drive) == FR_OK) {
                vfs_node_t node = {
                    .private_data = fs,
                    .ls = (void*)internal_fs_ls,
                    .cat = (void*)internal_fs_cat,
                    .write = (void*)internal_fs_write,
                    .mkdir = (void*)internal_fs_mkdir,
                    .rmdir = (void*)internal_fs_rmdir,
                    .exists = (void*)internal_fs_exists
                };
                int k = 0; while(name[k] && k < 15) { node.name[k] = name[k]; k++; } node.name[k] = '\0';
                vfs_register_node(node);
                *(int*)rdx = 0;
            } else {
                void release(void* ptr);
                release(fs);
                *(int*)rdx = -1;
            }
            break;
        }
    }
}
