#include <kernel/libs/core/services.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>

void vga_print(const char* fmt, ...);
void* bump_alloc(size_t size);
uint64_t vmm_get_phys(void* virt);
uint64_t get_hhdm_offset(void);

/* NVMe Controller Structure (Simplified) */
typedef struct {
    uint64_t cap;
    uint32_t vs;
    uint32_t intms;
    uint32_t intmc;
    uint32_t cc;
    uint32_t rsv0;
    uint32_t csts;
    uint32_t nssr;
    uint32_t aqa;
    uint64_t asq;
    uint64_t acq;
} nvme_regs_t;

typedef struct {
    uint32_t cdw0;
    uint32_t nsid;
    uint32_t rsvd[2];
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} nvme_sq_entry_t;

typedef struct {
    uint32_t res;
    uint32_t rsvd;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t cid;
    uint16_t status;
} nvme_cq_entry_t;

static nvme_regs_t* nvme_base = NULL;
static nvme_sq_entry_t* admin_sq = NULL;
static nvme_cq_entry_t* admin_cq = NULL;
static nvme_sq_entry_t* io_sq = NULL;
static nvme_cq_entry_t* io_cq = NULL;
static uint32_t admin_sq_tail = 0;
static uint32_t io_sq_tail = 0;
static uint16_t io_cq_head = 0;
static uint16_t io_phase = 0;
static uint32_t doorbell_stride = 0;

uint64_t get_system_ticks(void);

int nvme_wait_completion(nvme_cq_entry_t* cq, uint16_t head, uint16_t* last_phase) {
    uint64_t start = get_system_ticks();
    uint16_t expected_phase = !(*last_phase);
    while (get_system_ticks() - start < 1000) {
        if ((cq[head].status & 1) == expected_phase) {
            *last_phase = expected_phase;
            return 0;
        }
    }
    return -1; /* Timeout */
}

void nvme_write_doorbell(uint32_t qid, bool is_cq, uint32_t val) {
    uint32_t offset = (2 * qid + (is_cq ? 1 : 0)) * doorbell_stride;
    volatile uint32_t* doorbell = (volatile uint32_t*)((uint8_t*)nvme_base + 0x1000 + offset);
    *doorbell = val;
}

void nvme_setup_prp(nvme_sq_entry_t* entry, uint64_t phys_addr, size_t len) {
    entry->prp1 = phys_addr;
    if (len > 4096) {
        uint64_t* prp_list = (uint64_t*)bump_alloc(4096);
        entry->prp2 = vmm_get_phys(prp_list);
        for (int i = 0; i < (len / 4096); i++) {
            prp_list[i] = phys_addr + (i + 1) * 4096;
        }
    }
}

void nvme_submit_admin(nvme_sq_entry_t entry) {
    admin_sq[admin_sq_tail] = entry;
    admin_sq_tail = (admin_sq_tail + 1) % 64;
    nvme_write_doorbell(0, false, admin_sq_tail);
}

void nvme_submit_io(nvme_sq_entry_t entry) {
    io_sq[io_sq_tail] = entry;
    io_sq_tail = (io_sq_tail + 1) % 64;
    nvme_write_doorbell(1, false, io_sq_tail);
}

void nvme_reset_controller(void) {
    vga_print("[NVME] Resetting Controller...\n");
    nvme_base->cc &= ~1; /* Disable */
    while (nvme_base->csts & 1); /* Wait for Not Ready */
    admin_sq_tail = 0; io_sq_tail = 0; io_cq_head = 0; io_phase = 0;
    nvme_base->cc |= 1; /* Enable */
    while (!(nvme_base->csts & 1)); /* Wait for Ready */
    vga_print("[NVME] Reset Complete.\n");
}

int nvme_read_blocks(uint32_t nsid, uint64_t lba, uint32_t count, void* buffer) {
    for (int retry = 0; retry < 3; retry++) {
        nvme_sq_entry_t entry = { .cdw0 = 0x02, .nsid = nsid };
        nvme_setup_prp(&entry, vmm_get_phys(buffer), count * 512);
        entry.cdw10 = lba & 0xFFFFFFFF;
        entry.cdw11 = (lba >> 32) & 0xFFFFFFFF;
        entry.cdw12 = (count - 1) & 0xFFFF;
        nvme_submit_io(entry);

        uint16_t head = io_cq_head;
        if (nvme_wait_completion(io_cq, head, &io_phase) == 0) {
            io_cq_head = (io_cq_head + 1) % 64;
            nvme_write_doorbell(1, true, io_cq_head);
            return 0;
        }
    }
    return -1;
}

int nvme_write_blocks(uint32_t nsid, uint64_t lba, uint32_t count, void* buffer) {
    for (int retry = 0; retry < 3; retry++) {
        nvme_sq_entry_t entry = { .cdw0 = 0x01, .nsid = nsid };
        nvme_setup_prp(&entry, vmm_get_phys(buffer), count * 512);
        entry.cdw10 = lba & 0xFFFFFFFF;
        entry.cdw11 = (lba >> 32) & 0xFFFFFFFF;
        entry.cdw12 = (count - 1) & 0xFFFF;
        nvme_submit_io(entry);

        uint16_t head = io_cq_head;
        if (nvme_wait_completion(io_cq, head, &io_phase) == 0) {
            io_cq_head = (io_cq_head + 1) % 64;
            nvme_write_doorbell(1, true, io_cq_head);
            return 0;
        }
    }
    return -1;
}

void nvme_bind(uint8_t bus, uint8_t slot, uint8_t func) {
    vga_print("[NVME] Found NVMe Controller at %d:%d:%d\n", bus, slot, func);
    pci_enable_master(bus, slot, func);

    uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
    uint32_t bar1 = pci_config_read(bus, slot, func, 0x14);
    uint64_t phys_base = ((uint64_t)bar1 << 32) | (bar0 & 0xFFFFFFF0);

    nvme_base = (nvme_regs_t*)(get_hhdm_offset() + phys_base);
    vga_print("[NVME] Base Address: 0x%p\n", nvme_base);

    admin_sq = (nvme_sq_entry_t*)bump_alloc(4096);
    admin_cq = (nvme_cq_entry_t*)bump_alloc(4096);
    doorbell_stride = 4 << ((nvme_base->cap >> 32) & 0xF);

    nvme_base->aqa = (63 << 16) | 63; /* 64 entries each */
    nvme_base->asq = vmm_get_phys(admin_sq);
    nvme_base->acq = vmm_get_phys(admin_cq);

    nvme_base->cc = (0 << 16) | (0 << 14) | (4 << 7) | (6 << 4) | 1; /* Enable, 4KB page */
    while (!(nvme_base->csts & 1)); /* Wait for Ready */
    vga_print("[NVME] Controller Ready.\n");

    io_sq = (nvme_sq_entry_t*)bump_alloc(4096);
    io_cq = (nvme_cq_entry_t*)bump_alloc(4096);

    nvme_sq_entry_t create_cq = { .cdw0 = 0x05, .prp1 = vmm_get_phys(io_cq), .cdw10 = (63 << 16) | 1, .cdw11 = 1 };
    nvme_submit_admin(create_cq);
    nvme_sq_entry_t create_sq = { .cdw0 = 0x01, .prp1 = vmm_get_phys(io_sq), .cdw10 = (63 << 16) | 1, .cdw11 = (1 << 16) | 1 };
    nvme_submit_admin(create_sq);
}

void nvme_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        pci_driver_t nvme_driver = {
            .name = "NVMe Controller",
            .target = { .class_id = 0x01, .subclass = 0x08, .prog_if = 0x02 },
            .bind = nvme_bind
        };
        pci_register_driver(nvme_driver);
        vga_print("[INIT] NVMe PCI driver registered.\n");
    }
}
