#include <kernel/libs/core/services.h>
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
    /* Safety: Pattern to detect if this is a literal or managed object */
    /* In a real scenario, we'd check memory ranges. */
    arc_header_t* header = ((arc_header_t*)ptr) - 1;
    header->ref_count++;
}

void release(void* ptr) {
    if (!ptr) return;
    arc_header_t* header = ((arc_header_t*)ptr) - 1;
    if (header->ref_count > 0) {
        header->ref_count--;
    }
}
