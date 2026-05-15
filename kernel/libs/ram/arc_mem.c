#include <kernel/libs/core/services.h>
#include <stdint.h>
#include <stddef.h>
#include <include/stdlib.h>

/* Sovereign ARC Header: 16 bytes for alignment and property protection */
typedef struct {
    uint64_t ref_count;
    uint64_t magic;
} arc_header_t;

#define ARC_MAGIC 0x534F56524EULL /* "SOVRN" */

#include <kernel/unice64/task.h>
void serial_write_str(const char* s);
bool is_slab_pointer(void* ptr);

void arc_mem_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        serial_write_str("[INIT] ARC Memory Management active.\n");
    }
}

void* arc_alloc(size_t size) {
    /* Sovereign Allocation: 16-byte aligned header + payload */
    size_t total_size = size + sizeof(arc_header_t);
    arc_header_t* header = (arc_header_t*)malloc(total_size);
    if (!header) return NULL;

    header->ref_count = 1;
    header->magic = ARC_MAGIC;

    /* Return 16-byte aligned payload (if header was 16-byte aligned) */
    return (void*)(header + 1);
}

void retain(void* ptr) {
    if (!ptr || !is_slab_pointer(ptr)) return;
    arc_header_t* header = ((arc_header_t*)ptr) - 1;

    /* Vandalism Check: Ensure this is a Sovereign-managed object */
    if (header->magic == ARC_MAGIC) {
        header->ref_count++;
    }
}

void release(void* ptr) {
    if (!ptr || !is_slab_pointer(ptr)) return;
    arc_header_t* header = ((arc_header_t*)ptr) - 1;

    if (header->magic == ARC_MAGIC) {
        if (header->ref_count > 0) {
            if (--header->ref_count == 0) {
                header->magic = 0; /* Clear property line before reclamation */
                free(header);
            }
        }
    }
}
