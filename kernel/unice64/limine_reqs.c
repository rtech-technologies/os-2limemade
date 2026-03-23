#include <limine.h>
#include <stdint.h>
#include <stddef.h>

/* Limine Requests for Memory Map and HHDM */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

/* Provide access functions for other kernel parts */
struct limine_memmap_response* get_memmap(void) {
    return memmap_request.response;
}

uint64_t get_hhdm_offset(void) {
    return hhdm_request.response ? hhdm_request.response->offset : 0;
}
