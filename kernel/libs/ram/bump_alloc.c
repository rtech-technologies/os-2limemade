#include <include/config.h>
#include <stdint.h>
#include <stddef.h>

void* malloc(size_t size);

/* Simplified Bump Allocator for OSx2 Limemade Core */
#define HEAP_SIZE CONFIG_HEAP_SIZE

void serial_write_str(const char* s);

void bump_reset(void) {
    serial_write_str("[MEMORY] OSx2 Limemade Reset Triggered: Unloading all managed objects...\n");
    void slab_reset(int id);
    slab_reset(0);
}

void* bump_alloc(size_t size) {
    return malloc(size);
}
