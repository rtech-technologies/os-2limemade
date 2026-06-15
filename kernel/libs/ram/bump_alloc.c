#include <include/config.h>
#include <stdint.h>
#include <stddef.h>

void* malloc(size_t size);

/* Simplified Bump Allocator for OSx2 Limemade Core */
#define HEAP_SIZE CONFIG_HEAP_SIZE

void vga_print(const char* fmt, ...);

void bump_reset(void) {
    vga_print("[MEMORY] OSx2 Limemade Reset Triggered: Unloading all managed objects...\n");
    void slab_reset(int id);
    slab_reset(0);
}

void* bump_alloc(size_t size) {
    return malloc(size);
}
