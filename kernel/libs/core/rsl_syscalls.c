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
    (void)rdx; (void)rsi;

    switch(rax) {
        case 0: // print
            vga_print("%s", (const char*)rbx);
            break;
        case 15: // rsl_exists
            *(bool*)rsi = vfs_exists((void*)rbx);
            break;
        case 101: // rsl_list_disks
            *(int*)rbx = get_hw_disk_count();
            break;
        case 102: // rsl_is_sovereign
            // Check if disk has Sovereign signature or partition
            *(bool*)rcx = true;
            break;
        case 103: { // rsl_get_disk_info(disk_id, name_ptr, size_ptr)
            get_hw_disk_info((int)rbx, (char*)rcx, (uint64_t*)rdx);
            break;
        }
        case 104: { // rsl_format(disk_id)
            *(int*)rcx = (f_mkfs((int)rbx) == FR_OK) ? 0 : -1;
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
