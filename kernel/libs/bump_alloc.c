#include <include/config.h>
#include <stdint.h>
#include <stddef.h>

/* Simplified Bump Allocator for Sovereign Core */
#define HEAP_SIZE CONFIG_HEAP_SIZE

static uint8_t heap[HEAP_SIZE];
static size_t heap_offset = 0;

void serial_write_str(const char* s);

void bump_reset(void) {
    serial_write_str("[MEMORY] Sovereign Reset Triggered: Unloading all managed objects...\n");
    heap_offset = 0;
}

void* bump_alloc(size_t size) {
    /* Align to 8 bytes */
    size = (size + 7) & ~7;

    /* Sovereign Rule: Auto-Reset if close to end (90% threshold) */
    if (heap_offset + size > (HEAP_SIZE * 90 / 100)) {
        bump_reset();
    }

    if (heap_offset + size > HEAP_SIZE) {
        serial_write_str("[CRITICAL] Out of memory even after reset.\n");
        return NULL;
    }

    void* ptr = &heap[heap_offset];
    heap_offset += size;
    return ptr;
}
