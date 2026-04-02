#include <stdint.h>
#include <stddef.h>
#include <limine.h>

struct limine_memmap_response* get_memmap(void);
uint64_t get_hhdm_offset(void);

#define PAGE_SIZE 4096

static uint64_t total_pages = 0;
static uint64_t usable_pages = 0;
static uint8_t* bitmap = NULL;
static uint64_t bitmap_size = 0;

void pmm_init(void) {
    struct limine_memmap_response* memmap = get_memmap();
    uint64_t hhdm = get_hhdm_offset();
    uint64_t highest_addr = 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
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
