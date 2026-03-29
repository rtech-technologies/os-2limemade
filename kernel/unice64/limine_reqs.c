#include <limine.h>
#include <stdint.h>
#include <stddef.h>

/* Limine Requests for Memory Map and HHDM */
__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(0);

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

__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_boot_volume_request boot_volume_request = {
    .id = LIMINE_BOOT_VOLUME_REQUEST,
    .revision = 0
};

/* Provide access functions for other kernel parts */
struct limine_memmap_response* get_memmap(void) {
    return memmap_request.response;
}

uint64_t get_hhdm_offset(void) {
    return hhdm_request.response ? hhdm_request.response->offset : 0;
}

struct limine_module_response* get_modules(void) {
    return module_request.response;
}

struct limine_framebuffer_response* get_framebuffer(void) {
    return framebuffer_request.response;
}

struct limine_kernel_address_response* get_kernel_address(void) {
    return kernel_address_request.response;
}

struct limine_boot_volume_response* get_boot_volume(void) {
    return boot_volume_request.response;
}

uint64_t vmm_get_phys(void* virt) {
    uint64_t v = (uint64_t)virt;
    uint64_t hhdm = hhdm_request.response ? hhdm_request.response->offset : 0;
    struct limine_kernel_address_response* ka = kernel_address_request.response;

    /* Limine HHDM range check */
    if (hhdm != 0 && v >= hhdm && v < 0xffffffff80000000) {
        return v - hhdm;
    }

    /* Kernel range check */
    if (ka && v >= ka->virtual_base) {
        return v - ka->virtual_base + ka->physical_base;
    }

    /* Fallback for low-memory addresses if HHDM isn't available */
    return v;
}
