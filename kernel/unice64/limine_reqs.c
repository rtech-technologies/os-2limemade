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
static volatile struct limine_kernel_file_request kernel_file_request = {
    .id = LIMINE_KERNEL_FILE_REQUEST,
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
static volatile struct limine_bootloader_info_request bootloader_info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
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

struct limine_kernel_file_response* get_kernel_file(void) {
    return kernel_file_request.response;
}

struct limine_bootloader_info_response* get_bootloader_info(void) {
    return bootloader_info_request.response;
}

void* get_rsdp(void) {
    return rsdp_request.response ? rsdp_request.response->address : NULL;
}

uint64_t vmm_get_phys(void* virt) {
    uint64_t v = (uint64_t)virt;
    uint64_t hhdm = hhdm_request.response ? hhdm_request.response->offset : 0;
    struct limine_kernel_address_response* ka = kernel_address_request.response;

    /* 1. Kernel range check (must come first as it's a subset of high memory) */
    if (ka && v >= ka->virtual_base) {
        return v - ka->virtual_base + ka->physical_base;
    }

    /* 2. Limine HHDM range check */
    if (hhdm != 0 && v >= hhdm) {
        return v - hhdm;
    }

    /* 3. Fallback for low-memory addresses or absolute physical pointers */
    return v;
}
