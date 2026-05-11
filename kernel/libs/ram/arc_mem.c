#include <kernel/libs/core/services.h>
#include <stdint.h>
#include <stddef.h>
#include <include/stdlib.h>

/* ARC Header: 8 bytes */
typedef struct {
    uint64_t ref_count;
} arc_header_t;

#include <kernel/unice64/task.h>
void serial_write_str(const char* s);

void arc_mem_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        serial_write_str("[INIT] ARC Memory Management active.\n");
    }
}

void* arc_alloc(size_t size) {
    /* 🧱 Blocky Fix: Use malloc instead of raw slab_alloc to ensure header compatibility with free() */
    size_t total_size = size + sizeof(arc_header_t);
    arc_header_t* header = (arc_header_t*)malloc(total_size);
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
        if (header->ref_count == 0) {
            /* 🧱 Blocky Fix: Safely return the entire allocation (including arc_header) to system heap */
            free(header);
        }
    }
}
