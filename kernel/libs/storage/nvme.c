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

static nvme_regs_t* nvme_base = NULL;

void nvme_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        vga_print("[INIT] Scanning PCI for NVMe controllers...\n");
        for (int bus = 0; bus < 256; bus++) {
            for (int slot = 0; slot < 32; slot++) {
                for (int func = 0; func < 8; func++) {
                    uint32_t class_info = pci_config_read(bus, slot, func, 0x08);
                    uint8_t base_class = (class_info >> 24) & 0xFF;
                    uint8_t sub_class = (class_info >> 16) & 0xFF;
                    uint8_t prog_if = (class_info >> 8) & 0xFF;

                    if (base_class == 0x01 && sub_class == 0x08 && prog_if == 0x02) {
                        vga_print("[NVME] Found NVMe Controller at %d:%d:%d\n", bus, slot, func);

                        uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
                        uint32_t bar1 = pci_config_read(bus, slot, func, 0x14);
                        uint64_t phys_base = ((uint64_t)bar1 << 32) | (bar0 & 0xFFFFFFF0);

                        nvme_base = (nvme_regs_t*)(get_hhdm_offset() + phys_base);
                        vga_print("[NVME] Base Address: 0x%x, Version: 0x%x\n", (uint64_t)nvme_base, nvme_base->vs);
                        vga_print("[NVME] Capabilities: 0x%x\n", nvme_base->cap);

                        /* In a Sovereign OS, we would now map Admin Queues here */
                        return;
                    }
                }
            }
        }
    }
}
