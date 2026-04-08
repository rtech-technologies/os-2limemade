#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <include/panic.h>

uint64_t get_hhdm_offset(void);

#define SLAB_SIZE (4 * 1024 * 1024) /* 4MB Sovereign Slab */
#define MAX_SLABS 16

typedef struct slab_header {
    size_t size;
    bool is_used;
    uint8_t padding[7];
    struct slab_header* next;
    uint64_t reserved; /* 32-byte alignment */
} slab_header_t;

typedef struct {
    uintptr_t base;
    size_t offset;
    bool active;
    bool in_use;
    slab_header_t* first_block;
} sovereign_slab_t;

static sovereign_slab_t slabs[MAX_SLABS];
static int slab_count = 0;

void* pmm_alloc(uint64_t count);

void slab_init(void) {
    /* Sovereign Partitioning: Divide memory into 4MB slabs */
    for (int i = 0; i < MAX_SLABS; i++) {
        void* ptr = pmm_alloc(SLAB_SIZE / 4096);
        PANIC_ON(ptr == NULL, "SLAB_INIT: PHYSICAL MEMORY DEPLETED");

        slabs[i].base = (uintptr_t)ptr;
        slabs[i].offset = 0;
        slabs[i].active = false;
        slabs[i].in_use = (i < 3); /* 0=Idle, 1=System, 2=Shell reserved */
        slab_count++;
    }
}

int slab_grab_transient(void) {
    for (int i = 3; i < MAX_SLABS; i++) {
        if (!slabs[i].in_use) {
            slabs[i].in_use = true;
            slabs[i].offset = 0;
            return i;
        }
    }
    return -1;
}

void slab_release_transient(int id) {
    if (id >= 3 && id < MAX_SLABS) {
        slabs[id].in_use = false;
        slabs[id].offset = 0;
    }
}

void* slab_get_base(int id) {
    if (id < 0 || id >= MAX_SLABS) return NULL;
    uint64_t hhdm = get_hhdm_offset();
    return (void*)(slabs[id].base + hhdm);
}

void* slab_alloc_aligned(int id, size_t size, size_t align) {
    if (id < 0 || id >= MAX_SLABS) return NULL;

    uint64_t hhdm = get_hhdm_offset();
    uintptr_t current_addr = slabs[id].base + hhdm + slabs[id].offset;

    uintptr_t aligned_addr = (current_addr + (align - 1)) & ~(align - 1);
    size_t padding = aligned_addr - current_addr;

    if (slabs[id].offset + padding + size > SLAB_SIZE) return NULL;

    slabs[id].offset += padding + size;
    return (void*)aligned_addr;
}

void* slab_alloc(int id, size_t size) {
    return slab_alloc_aligned(id, size, 16);
}

void* malloc(size_t size);
void free(void* ptr);

/* Permanent Buffers: Used by core kernel subsystems (like FatFS) to avoid recycling */
static uint8_t fatfs_buffer_shield[2048];
static bool fatfs_shield_in_use = false;

void* slab_alloc_persistent(size_t size) {
    if (size <= 2048 && !fatfs_shield_in_use) {
        fatfs_shield_in_use = true;
        return fatfs_buffer_shield;
    }
    return malloc(size); /* Fallback to recycling heap if shield busy */
}

void slab_free_persistent(void* ptr) {
    if (ptr == fatfs_buffer_shield) {
        fatfs_shield_in_use = false;
    } else {
        free(ptr);
    }
}

void slab_reset(int id) {
    if (id >= 0 && id < MAX_SLABS) slabs[id].offset = 0;
}

size_t slab_get_usage(int id) {
    if (id >= 0 && id < MAX_SLABS) return slabs[id].offset;
    return 0;
}

void* malloc_ext(int id, size_t size) {
    if (id < 0 || id >= MAX_SLABS) return NULL;
    size = (size + 15) & ~15; /* Align */

    slab_header_t* search = slabs[id].first_block;
    while (search) {
        if (!search->is_used && search->size >= size) {
            search->is_used = true;
            return (void*)((uint8_t*)search + sizeof(slab_header_t));
        }
        search = search->next;
    }

    /* No free block found, bump allocate new block from requested slab */
    slab_header_t* new_block = (slab_header_t*)slab_alloc_aligned(id, size + sizeof(slab_header_t), 32);
    if (!new_block) return NULL;

    new_block->size = size;
    new_block->is_used = true;
    new_block->next = slabs[id].first_block;
    slabs[id].first_block = new_block;

    return (void*)((uint8_t*)new_block + sizeof(slab_header_t));
}

void* malloc(size_t size) {
    return malloc_ext(0, size);
}

void free(void* ptr) {
    if (!ptr) return;
    slab_header_t* header = (slab_header_t*)((uint8_t*)ptr - sizeof(slab_header_t));
    header->is_used = false;
}
