#include <kernel/libs/core/services.h>
#include <stdint.h>
#include <stddef.h>

/* ARC Header: 8 bytes */
typedef struct {
    uint64_t ref_count;
} arc_header_t;

void* slab_alloc(int id, size_t size);
#include <kernel/unice64/task.h>
void serial_write_str(const char* s);

void arc_mem_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        serial_write_str("[INIT] ARC Memory Management active.\n");
    }
}

void* malloc_ext(int id, size_t size);

void* arc_alloc(size_t size) {
    size_t total_size = size + sizeof(arc_header_t);
    task_t* current = get_current_task();
    int slab_id = current ? current->slab_id : 0;

    /* Use Slab-specific recycling heap for ARC objects */
    arc_header_t* header = (arc_header_t*)malloc_ext(slab_id, total_size);
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

void free(void* ptr);

void release(void* ptr) {
    if (!ptr) return;
    arc_header_t* header = ((arc_header_t*)ptr) - 1;
    if (header->ref_count > 0) {
        header->ref_count--;
        if (header->ref_count == 0) {
            /* Deterministic recycling via Slab-based free */
            free(header);
        }
    }
}
