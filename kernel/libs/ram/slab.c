#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

uint64_t get_hhdm_offset(void);

#define SLAB_SIZE (4 * 1024 * 1024) /* 4MB Sovereign Slab */
#define MAX_SLABS 16

typedef struct {
    uintptr_t base;
    size_t offset;
    bool active;
} sovereign_slab_t;

static sovereign_slab_t slabs[MAX_SLABS];
static int slab_count = 0;

void* pmm_alloc(uint64_t count);

void slab_init(void) {
    /* Sovereign Partitioning: Divide memory into 4MB slabs */
    for (int i = 0; i < MAX_SLABS; i++) {
        void* ptr = pmm_alloc(SLAB_SIZE / 4096);
        if (ptr) {
            slabs[i].base = (uintptr_t)ptr;
            slabs[i].offset = 0;
            slabs[i].active = false;
            slab_count++;
        }
    }
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

void slab_reset(int id) {
    if (id >= 0 && id < MAX_SLABS) slabs[id].offset = 0;
}

size_t slab_get_usage(int id) {
    if (id >= 0 && id < MAX_SLABS) return slabs[id].offset;
    return 0;
}

void* malloc(size_t size) {
    /* Use Slab 0 as Global System Heap */
    return slab_alloc(0, size);
}

void free(void* ptr) {
    /* Deterministic recycling via ARC logic.
       In a pure bump/slab model, we don't free individual items
       unless we integrate a freelist, but Rule #4 says we use Slab
       partitioning with deterministic cleanup. */
    (void)ptr;
}
