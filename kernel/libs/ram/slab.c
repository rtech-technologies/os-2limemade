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
    uint64_t reserved;
} slab_header_t;

typedef struct {
    uintptr_t base; /* Physical Base */
    size_t offset;
    bool active;
    bool in_use;
    slab_header_t* first_block;
} sovereign_slab_t;

static sovereign_slab_t slabs[MAX_SLABS];
static int slab_count = 0;

void* pmm_alloc(uint64_t count);
void* slab_get_base(int id);

void slab_init(void) {
    void vga_print(const char* fmt, ...);
    vga_print("[SLAB] Initializing %d Sovereign Slabs (4MB each)...\n", MAX_SLABS);
    for (int i = 0; i < MAX_SLABS; i++) {
        void* ptr = pmm_alloc(SLAB_SIZE / 4096);
        PANIC_ON(ptr == NULL, "SLAB_INIT: PHYSICAL MEMORY DEPLETED");

        slabs[i].base = (uintptr_t)ptr;
        slabs[i].offset = 0;
        slabs[i].active = false;
        slabs[i].in_use = (i < 5);
        slab_count++;
        vga_print("[SLAB] Slab %d: Phys 0x%x, Virt 0x%x\n", i, (uint64_t)slabs[i].base, (uint64_t)slab_get_base(i));
    }
}

int slab_grab_transient(void) {
    for (int i = 5; i < MAX_SLABS; i++) {
        if (!slabs[i].in_use) {
            slabs[i].in_use = true; slabs[i].offset = 0; return i;
        }
    }
    return -1;
}

void slab_release_transient(int id) {
    if (id >= 5 && id < MAX_SLABS) { slabs[id].in_use = false; slabs[id].offset = 0; }
}

void* slab_get_base(int id) {
    if (id < 0 || id >= MAX_SLABS) return NULL;
    uint64_t hhdm = get_hhdm_offset();
    return (void*)(slabs[id].base + hhdm);
}

void* slab_alloc_aligned(int id, size_t size, size_t align) {
    if (id < 0 || id >= MAX_SLABS) return NULL;
    uint64_t hhdm = get_hhdm_offset();
    uintptr_t current_virt = slabs[id].base + hhdm + slabs[id].offset;
    uintptr_t aligned_virt = (current_virt + (align - 1)) & ~(align - 1);
    size_t padding = aligned_virt - current_virt;

    if (slabs[id].offset + padding + size > SLAB_SIZE) return NULL;
    slabs[id].offset += padding + size;
    return (void*)aligned_virt;
}

/* 🎯 Sentry Fix: Explicit Physical Addressing Support */
void* slab_alloc_phys(int id, size_t size, size_t align, uint64_t* out_phys) {
    void* virt = slab_alloc_aligned(id, size, align);
    if (!virt) return NULL;
    if (out_phys) {
        uint64_t offset_in_slab = (uint64_t)virt - (uint64_t)slab_get_base(id);
        *out_phys = slabs[id].base + offset_in_slab;
    }
    return virt;
}

void* slab_alloc(int id, size_t size) {
    return slab_alloc_aligned(id, size, 16); /* 🎯 Sentry Fix: Strict 16-byte alignment for SSE */
}

void* malloc_ext(int id, size_t size) {
    if (id < 0 || id >= MAX_SLABS) return NULL;
    size = (size + 15) & ~15;
    slab_header_t* search = slabs[id].first_block;
    while (search) {
        if (!search->is_used && search->size >= size) { search->is_used = true; return (void*)((uint8_t*)search + sizeof(slab_header_t)); }
        search = search->next;
    }
    slab_header_t* new_block = (slab_header_t*)slab_alloc_aligned(id, size + sizeof(slab_header_t), 32);
    if (!new_block) return NULL;
    new_block->size = size; new_block->is_used = true; new_block->next = slabs[id].first_block; slabs[id].first_block = new_block;
    return (void*)((uint8_t*)new_block + sizeof(slab_header_t));
}

void* malloc(size_t size) { return malloc_ext(0, size); }
void free(void* ptr) {
    if (!ptr) return;
    slab_header_t* header = (slab_header_t*)((uint8_t*)ptr - sizeof(slab_header_t));
    header->is_used = false;
}

#define SHIELD_DEPTH 4
static uint8_t fatfs_buffer_shields[SHIELD_DEPTH][2048] __attribute__((aligned(16)));
static bool fatfs_shields_in_use[SHIELD_DEPTH] = {false, false, false, false};

void* slab_alloc_persistent(size_t size) {
    if (size <= 2048) {
        for (int i = 0; i < SHIELD_DEPTH; i++) {
            if (!fatfs_shields_in_use[i]) { fatfs_shields_in_use[i] = true; return fatfs_buffer_shields[i]; }
        }
    }
    return malloc(size);
}

void slab_free_persistent(void* ptr) {
    for (int i = 0; i < SHIELD_DEPTH; i++) {
        if (ptr == fatfs_buffer_shields[i]) { fatfs_shields_in_use[i] = false; return; }
    }
    free(ptr);
}

void slab_reset(int id) { if (id >= 0 && id < MAX_SLABS) slabs[id].offset = 0; }
size_t slab_get_usage(int id) { if (id >= 0 && id < MAX_SLABS) return slabs[id].offset; return 0; }
