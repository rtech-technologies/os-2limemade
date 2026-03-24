#include <include/config.h>
#include <stdint.h>
#include <stddef.h>

/* Simplified Bump Allocator for Sovereign Core */
#define HEAP_SIZE CONFIG_HEAP_SIZE

static uint8_t heap[HEAP_SIZE];
static size_t heap_offset = 0;

void* bump_alloc(size_t size) {
    /* Align to 8 bytes */
    size = (size + 7) & ~7;

    if (heap_offset + size > HEAP_SIZE) {
        return NULL; /* Out of memory */
    }

    void* ptr = &heap[heap_offset];
    heap_offset += size;
    return ptr;
}

void bump_reset(void) {
    heap_offset = 0;
}
