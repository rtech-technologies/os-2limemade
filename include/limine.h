#ifndef LIMINE_H
#define LIMINE_H

#include <stdint.h>

#define LIMINE_BASE_REVISION(N) \
    uint64_t limine_base_revision[3] = { 0xf95627f307074494, 0x7b62640244799044, (N) }

#define LIMINE_MEMMAP_REQUEST { 0xc7b1dd30df4c8b88, 0x0a82e883a194f07b }
#define LIMINE_KERNEL_FILE_REQUEST { 0xad97e90e83f1c678, 0x7e5960523b567d2a }
#define LIMINE_HHDM_REQUEST { 0xb0ed697e44cadc28, 0x929dc5b52c38466e }
#define LIMINE_MODULE_REQUEST { 0x3e7e2797026feb3b, 0xd65330e793931495 }
#define LIMINE_FRAMEBUFFER_REQUEST { 0x9d582a93325e6378, 0x1d467727402f7850 }
#define LIMINE_KERNEL_ADDRESS_REQUEST { 0x71ba76863d25f63d, 0xd3734d4651d962c2 }
#define LIMINE_BOOTLOADER_INFO_REQUEST { 0xf55038d840120ae0, 0x2749073b22c81ad6 }
#define LIMINE_RSDP_REQUEST { 0xc5e77b03b30f0896, 0x1d8f331405b0728c }

struct limine_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

#define LIMINE_MEMMAP_USABLE                 0
#define LIMINE_MEMMAP_RESERVED               1
#define LIMINE_MEMMAP_ACPI_RECLAIMABLE       2
#define LIMINE_MEMMAP_ACPI_NVS               3
#define LIMINE_MEMMAP_BAD_MEMORY             4
#define LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE 5
#define LIMINE_MEMMAP_KERNEL_AND_MODULES     6
#define LIMINE_MEMMAP_FRAMEBUFFER            7

struct limine_memmap_response {
    uint64_t revision;
    uint64_t entry_count;
    struct limine_memmap_entry **entries;
};

struct limine_memmap_request {
    uint64_t id[2];
    uint64_t revision;
    struct limine_memmap_response *response;
};

struct limine_hhdm_response {
    uint64_t revision;
    uint64_t offset;
};

struct limine_hhdm_request {
    uint64_t id[2];
    uint64_t revision;
    struct limine_hhdm_response *response;
};

struct limine_file {
    uint64_t revision;
    void *address;
    uint64_t size;
    char *path;
    char *cmdline;
    uint32_t media_type;
    uint32_t unused;
    uint32_t tftp_ip;
    uint32_t tftp_port;
    uint32_t partition_index;
    uint32_t mbr_disk_id;
    uint8_t gpt_disk_uuid[16];
    uint8_t gpt_part_uuid[16];
    uint8_t part_uuid[16];
};

struct limine_kernel_file_response {
    uint64_t revision;
    struct limine_file *kernel_file;
};

struct limine_kernel_file_request {
    uint64_t id[2];
    uint64_t revision;
    struct limine_kernel_file_response *response;
};

struct limine_module_response {
    uint64_t revision;
    uint64_t module_count;
    struct limine_file **modules;
};

struct limine_module_request {
    uint64_t id[2];
    uint64_t revision;
    struct limine_module_response *response;
};

struct limine_framebuffer {
    void *address;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t unused[7];
    uint64_t edid_size;
    void *edid;
};

struct limine_framebuffer_response {
    uint64_t revision;
    uint64_t framebuffer_count;
    struct limine_framebuffer **framebuffers;
};

struct limine_framebuffer_request {
    uint64_t id[2];
    uint64_t revision;
    struct limine_framebuffer_response *response;
};

struct limine_kernel_address_response {
    uint64_t revision;
    uint64_t physical_base;
    uint64_t virtual_base;
};

struct limine_kernel_address_request {
    uint64_t id[2];
    uint64_t revision;
    struct limine_kernel_address_response *response;
};

struct limine_bootloader_info_response {
    uint64_t revision;
    char *name;
    char *version;
};

struct limine_bootloader_info_request {
    uint64_t id[2];
    uint64_t revision;
    struct limine_bootloader_info_response *response;
};

struct limine_rsdp_response {
    uint64_t revision;
    void *address;
};

struct limine_rsdp_request {
    uint64_t id[2];
    uint64_t revision;
    struct limine_rsdp_response *response;
};

#endif
