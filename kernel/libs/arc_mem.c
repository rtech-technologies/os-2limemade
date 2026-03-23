#include <kernel/libs/services.h>
#include <stdint.h>
#include <stddef.h>

/* ARC Header: 8 bytes */
typedef struct {
    uint64_t ref_count;
} arc_header_t;

void* bump_alloc(size_t size);
void serial_write_str(const char* s);

void arc_mem_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        serial_write_str("[INIT] ARC Memory Management active.\n");
    } else if (event == EVENT_CLEANUP) {
        serial_write_str("[CLEANUP] Freeing managed Sovereign memory...\n");
        /* In a bump allocator, cleanup is just resetting the offset if needed,
           but Sovereign rules suggest memory stays allocated until CLEANUP. */
    }
}

void* arc_alloc(size_t size) {
    size_t total_size = size + sizeof(arc_header_t);
    arc_header_t* header = (arc_header_t*)bump_alloc(total_size);
    if (!header) return NULL;

    header->ref_count = 1; /* Initial reference count */
    return (void*)(header + 1);
}

void retain(void* ptr) {
    if (!ptr) return;
    arc_header_t* header = ((arc_header_t*)ptr) - 1;
    header->ref_count++;
}

void release(void* ptr) {
    if (!ptr) return;
    arc_header_t* header = ((arc_header_t*)ptr) - 1;
    if (header->ref_count > 0) {
        header->ref_count--;
    }

    if (header->ref_count == 0) {
        /* In this bump allocator implementation, we don't actually free back
           to the heap immediately. Rule #4: manual release() required for
           future-proofing. Memory stays Sovereign until EVENT_CLEANUP. */
    }
}
