#include <stdint.h>
#include <stddef.h>
#include <limine.h>

struct limine_memmap_response* get_memmap(void);
uint64_t get_hhdm_offset(void);
void vga_print(const char* fmt, ...);

#define PAGE_SIZE 4096

static uint64_t total_pages = 0;
static uint64_t usable_pages = 0;
static uint8_t* bitmap = NULL;
static uint64_t bitmap_size = 0;

void pmm_init(void) {
    struct limine_memmap_response* memmap = get_memmap();
    uint64_t hhdm = get_hhdm_offset();
    uint64_t highest_addr = 0;

    vga_print("[PMM] System Memory Map:\n");
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        const char* type_str = "UNKNOWN";
        switch (entry->type) {
            case LIMINE_MEMMAP_USABLE: type_str = "USABLE"; break;
            case LIMINE_MEMMAP_RESERVED: type_str = "RESERVED"; break;
            case LIMINE_MEMMAP_ACPI_RECLAIMABLE: type_str = "ACPI RECLAIM"; break;
            case LIMINE_MEMMAP_ACPI_NVS: type_str = "ACPI NVS"; break;
            case LIMINE_MEMMAP_BAD_MEMORY: type_str = "BAD"; break;
            case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: type_str = "BOOT RECLAIM"; break;
            case LIMINE_MEMMAP_KERNEL_AND_MODULES: type_str = "KERNEL/MODS"; break;
            case LIMINE_MEMMAP_FRAMEBUFFER: type_str = "FRAMEBUFFER"; break;
        }
        vga_print("  [0x%x - 0x%x] %s (%d KB)\n",
                  entry->base, entry->base + entry->length, type_str, entry->length / 1024);

        if (entry->base + entry->length > highest_addr) {
            highest_addr = entry->base + entry->length;
        }
    }

    total_pages = highest_addr / PAGE_SIZE;
    bitmap_size = (total_pages / 8) + 1;

    /* Find a spot for the bitmap */
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= bitmap_size) {
            bitmap = (uint8_t*)(hhdm + entry->base);
            /* Mark bitmap area as used */
            for (uint64_t j = 0; j < bitmap_size; j++) bitmap[j] = 0xFF;
            entry->base += (bitmap_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            entry->length -= (bitmap_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            break;
        }
    }

    /* Initialize all pages as used */
    for (uint64_t i = 0; i < bitmap_size; i++) bitmap[i] = 0xFF;

    /* GOP Shield: Reserve Framebuffer range in the Bitmap */
    struct limine_framebuffer_response* get_framebuffer(void);
    uint64_t vmm_get_phys(void* virt);
    struct limine_framebuffer_response* fb_resp = get_framebuffer();
    if (fb_resp && fb_resp->framebuffer_count > 0) {
        struct limine_framebuffer* fb = fb_resp->framebuffers[0];
        uint64_t fb_phys = vmm_get_phys(fb->address);
        uint64_t fb_pages = (fb->pitch * fb->height + PAGE_SIZE - 1) / PAGE_SIZE;
        uint64_t start_page = fb_phys / PAGE_SIZE;
        for (uint64_t i = 0; i < fb_pages; i++) {
            if (start_page + i < total_pages) {
                bitmap[(start_page + i) / 8] |= (1 << ((start_page + i) % 8));
            }
        }
    }

    /* Free usable regions in the bitmap */
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            for (uint64_t j = 0; j < entry->length; j += PAGE_SIZE) {
                uint64_t page = (entry->base + j) / PAGE_SIZE;
                bitmap[page / 8] &= ~(1 << (page % 8));
                usable_pages++;
            }
        }
    }

    vga_print("[PMM] Physical Memory Manager initialized.\n");
    vga_print("[PMM] Total Pages: %d (%d MB)\n", total_pages, (total_pages * PAGE_SIZE) / 1024 / 1024);
    vga_print("[PMM] Usable Pages: %d (%d MB)\n", usable_pages, (usable_pages * PAGE_SIZE) / 1024 / 1024);
    vga_print("[PMM] Bitmap location: 0x%x, size: %d bytes\n", (uint64_t)bitmap, bitmap_size);
}

void* pmm_alloc(uint64_t count) {
    uint64_t found = 0;
    uint64_t start_page = 0;

    for (uint64_t i = 0; i < total_pages; i++) {
        if (!(bitmap[i / 8] & (1 << (i % 8)))) {
            if (found == 0) start_page = i;
            found++;
            if (found == count) {
                for (uint64_t j = 0; j < count; j++) {
                    bitmap[(start_page + j) / 8] |= (1 << ((start_page + j) % 8));
                }
                return (void*)(start_page * PAGE_SIZE);
            }
        } else {
            found = 0;
        }
    }
    return NULL;
}

void pmm_free(void* ptr, uint64_t count) {
    uint64_t start_page = (uint64_t)ptr / PAGE_SIZE;
    for (uint64_t i = 0; i < count; i++) {
        bitmap[(start_page + i) / 8] &= ~(1 << ((start_page + i) % 8));
    }
}
