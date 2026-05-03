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

void rsl_syscall_handler(uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx, uint64_t rsi) {
    switch(rax) {
        case 0: // print
            vga_print("%s", (const char*)rbx);
            sys_yield();
            break;
        case 1: { // rsl_input(prompt, buffer)
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
        case 15: // rsl_exists
            *(bool*)rsi = vfs_exists((void*)rbx);
            break;
        case 101: // rsl_list_disks
            *(int*)rbx = get_hw_disk_count();
            break;
        case 102: { // rsl_is_sovereign
            int drive = (int)rbx;
            uint8_t sector[512];
            bool* out = (bool*)rcx;
            *out = false;
            /* Check 1: FAT32 Label in BPB at LBA 2048 */
            if (disk_read(drive, sector, 2048, 1) == RES_OK) {
                if (sector[510] == 0x55 && sector[511] == 0xAA) {
                    /* Check OEM Name or Label */
                    if (sector[3] == 'O' && sector[4] == 'S' && sector[5] == 'X') *out = true;
                }
            }
            /* Check 2: GPT Partition Name "Sovereign" at LBA 2 */
            if (!*out && disk_read(drive, sector, 2, 1) == RES_OK) {
                /* GPT Entry 1 Name is at offset 56, "Sovereign" in UTF-16LE */
                if (sector[56] == 'S' && sector[58] == 'o') *out = true;
            }
            break;
        }
        case 103: { // rsl_get_disk_info(disk_id, name_ptr, size_ptr)
            get_hw_disk_info((int)rbx, (char*)rcx, (uint64_t*)rdx);
            break;
        }
        case 104: { // rsl_format(disk_id)
            *(int*)rcx = (f_mkfs((int)rbx) == FR_OK) ? 0 : -1;
            break;
        }
        case 105: { // rsl_fdisk(disk_id)
            *(int*)rcx = (f_fdisk((int)rbx) == FR_OK) ? 0 : -1;
            break;
        }
        case 110: { // rsl_set_uid(uid)
            task_t* current = get_current_task();
            /* Quartermaster: Security. Only UID 0 can set other UIDs */
            if (current && current->uid == 0) current->uid = (uint32_t)rbx;
            break;
        }
        case 111: { // rsl_get_uid()
            task_t* current = get_current_task();
            if (current) *(uint32_t*)rbx = current->uid;
            break;
        }
        case 120: { // rsl_write(path, content)
            vfs_write_dispatch((void*)rbx, (void*)rcx);
            break;
        }
        case 121: { // rsl_mkdir(path)
            vfs_mkdir((void*)rbx);
            break;
        }
        case 122: { // rsl_open(path, mode)
            *(vfs_handle_t**)rdx = vfs_open((void*)rbx, (const char*)rcx);
            break;
        }
        case 123: { // rsl_read(handle, buf, len)
            *(int*)rdx = vfs_read((vfs_handle_t*)rbx, (void*)rcx, (int)rsi);
            break;
        }
        case 124: { // rsl_close(handle)
            vfs_close((vfs_handle_t*)rbx);
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
    }
}
