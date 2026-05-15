#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

struct limine_memmap_response* get_memmap(void);
uint64_t get_hhdm_offset(void);
uint64_t vmm_get_phys(void* virt);

#define PAGE_SIZE 4096

static uint64_t total_pages = 0;
static uint64_t usable_pages = 0;
static uint8_t* bitmap = NULL;
static uint64_t bitmap_size = 0;

static void pmm_reserve_phys(uint64_t phys, uint64_t size) {
    uint64_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t start_page = phys / PAGE_SIZE;
    for (uint64_t i = 0; i < pages; i++) {
        uint64_t page = start_page + i;
        if (page < total_pages) bitmap[page / 8] |= (1 << (page % 8));
    }
}

void pmm_init(void) {
    struct limine_memmap_response* memmap = get_memmap();
    uint64_t hhdm = get_hhdm_offset();
    uint64_t highest_addr = 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        uint64_t end = memmap->entries[i]->base + memmap->entries[i]->length;
        if (end > highest_addr) highest_addr = end;
    }

    total_pages = highest_addr / PAGE_SIZE;
    bitmap_size = (total_pages / 8) + 1;

    /* 🧱 Sovereign Property Shield: Modules and Metadata Protection */
    struct limine_module_response* get_modules(void);
    struct limine_module_response* m_resp = get_modules();

    /* Find a safe usable spot for the bitmap */
    bitmap = NULL;
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= bitmap_size) {
            /* Verify no collision with modules in this usable region */
            uint64_t cand_phys = entry->base;
            uint64_t cand_end = cand_phys + bitmap_size;
            bool conflict = false;

            if (m_resp) {
                for (uint64_t m = 0; m < m_resp->module_count; m++) {
                    uint64_t m_phys = (uint64_t)m_resp->modules[m]->address - hhdm;
                    uint64_t m_end = m_phys + m_resp->modules[m]->size;
                    if (!(cand_end <= m_phys || cand_phys >= m_end)) { conflict = true; break; }
                }
            }
            if (!conflict) {
                bitmap = (uint8_t*)(hhdm + cand_phys);
                break;
            }
        }
    }

    if (!bitmap) {
        /* Fallback: try any usable region if first check failed */
        for (uint64_t i = 0; i < memmap->entry_count; i++) {
            if (memmap->entries[i]->type == LIMINE_MEMMAP_USABLE && memmap->entries[i]->length >= bitmap_size) {
                bitmap = (uint8_t*)(hhdm + memmap->entries[i]->base);
                break;
            }
        }
    }

    /* Phase 1: Default to fully occupied (Respect Property) */
    for (uint64_t i = 0; i < bitmap_size; i++) bitmap[i] = 0xFF;

    /* Phase 2: Open up USABLE regions */
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

    /* Phase 3: Final Reservation of Ownership */
    pmm_reserve_phys((uint64_t)bitmap - hhdm, bitmap_size);

    struct limine_framebuffer_response* get_framebuffer(void);
    struct limine_framebuffer_response* fb_resp = get_framebuffer();
    if (fb_resp && fb_resp->framebuffer_count > 0) {
        pmm_reserve_phys(vmm_get_phys(fb_resp->framebuffers[0]->address),
                         fb_resp->framebuffers[0]->pitch * fb_resp->framebuffers[0]->height);
    }

    if (m_resp) {
        /* Reserve Module Headers, Array, and Payload */
        pmm_reserve_phys(vmm_get_phys(m_resp), sizeof(*m_resp));
        pmm_reserve_phys(vmm_get_phys(m_resp->modules), m_resp->module_count * sizeof(void*));
        for (uint64_t i = 0; i < m_resp->module_count; i++) {
            struct limine_file* mod = m_resp->modules[i];
            pmm_reserve_phys(vmm_get_phys(mod->address), mod->size);
            pmm_reserve_phys(vmm_get_phys(mod), sizeof(struct limine_file));
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
