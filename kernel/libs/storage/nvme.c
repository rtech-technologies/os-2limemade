#include <kernel/libs/core/services.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>

void vga_print(const char* fmt, ...);
uint64_t get_hhdm_offset(void);

typedef struct {
    uint32_t cdw0; uint32_t nsid; uint64_t rsv0; uint64_t metadata; uint64_t prp1; uint64_t prp2;
    uint32_t cdw10; uint32_t cdw11; uint32_t cdw12; uint32_t cdw13; uint32_t cdw14; uint32_t cdw15;
} nvme_sqe_t;

typedef struct {
    uint32_t res; uint32_t rsv0; uint16_t sq_head; uint16_t sq_id; uint16_t command_id; uint16_t status;
} nvme_cqe_t;

typedef struct {
    uint64_t cap; uint32_t vs; uint32_t intms; uint32_t intmc; uint32_t cc; uint32_t rsv0;
    uint32_t csts; uint32_t nssr; uint32_t aqa; uint64_t asq; uint64_t acq;
} nvme_regs_t;

typedef struct {
    nvme_regs_t* regs;
    uint32_t* doorbells;
    nvme_sqe_t* asq;
    nvme_cqe_t* acq;
    nvme_sqe_t* sq;
    nvme_cqe_t* cq;
    uint16_t asq_tail; uint16_t acq_head; uint16_t sq_tail; uint16_t cq_head;
    uint16_t acq_phase; uint16_t cq_phase;
} nvme_ctrl_t;

static nvme_ctrl_t g_nvme;

void* pmm_alloc(uint64_t count);
uint64_t vmm_get_phys(void* virt);

static void local_memset(void* ptr, int val, size_t size) { uint8_t* p = (uint8_t*)ptr; while (size--) *p++ = (uint8_t)val; }

int nvme_submit_admin_cmd(nvme_sqe_t* cmd, nvme_cqe_t* res) {
    uint32_t dstrd = (g_nvme.regs->cap >> 32) & 0xF;
    g_nvme.asq[g_nvme.asq_tail] = *cmd;
    g_nvme.asq_tail = (g_nvme.asq_tail + 1) % 64;
    g_nvme.doorbells[0] = g_nvme.asq_tail;
    int timeout = 1000000;
    while ((g_nvme.acq[g_nvme.acq_head].status & 1) != g_nvme.acq_phase && timeout--) __asm__ volatile("pause");
    if (timeout <= 0) return -1;
    *res = g_nvme.acq[g_nvme.acq_head];
    g_nvme.acq_head = (g_nvme.acq_head + 1) % 64;
    if (g_nvme.acq_head == 0) g_nvme.acq_phase ^= 1;
    g_nvme.doorbells[1 << dstrd] = g_nvme.acq_head;
    return (res->status >> 1);
}

int nvme_read(void* priv, uint64_t lba, uint32_t count, void* buf) {
    (void)priv;
    uint32_t dstrd = (g_nvme.regs->cap >> 32) & 0xF;
    nvme_sqe_t cmd = { .cdw0 = 2, .nsid = 1, .prp1 = vmm_get_phys(buf), .cdw10 = (uint32_t)lba, .cdw11 = (uint32_t)(lba >> 32), .cdw12 = count - 1 };
    g_nvme.sq[g_nvme.sq_tail] = cmd;
    g_nvme.sq_tail = (g_nvme.sq_tail + 1) % 64;
    g_nvme.doorbells[2 << dstrd] = g_nvme.sq_tail;
    int timeout = 1000000;
    while ((g_nvme.cq[g_nvme.cq_head].status & 1) != g_nvme.cq_phase && timeout--) __asm__ volatile("pause");
    if (timeout <= 0) return -1;
    g_nvme.cq_head = (g_nvme.cq_head + 1) % 64;
    if (g_nvme.cq_head == 0) g_nvme.cq_phase ^= 1;
    g_nvme.doorbells[3 << dstrd] = g_nvme.cq_head;
    return 0;
}

void nvme_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        for (int bus = 0; bus < 256; bus++) {
            for (int slot = 0; slot < 32; slot++) {
                for (int func = 0; func < 8; func++) {
                    uint32_t id = pci_config_read(bus, slot, func, 0x08);
                    if (((id >> 24) & 0xFF) == 0x01 && ((id >> 16) & 0xFF) == 0x08) {
                        pci_enable_master(bus, slot, func);
                        uint32_t b0 = pci_config_read(bus, slot, func, 0x10);
                        uint32_t b1 = pci_config_read(bus, slot, func, 0x14);
                        g_nvme.regs = (nvme_regs_t*)(get_hhdm_offset() + (((uint64_t)b1 << 32) | (b0 & 0xFFFFFFF0)));
                        g_nvme.doorbells = (uint32_t*)((uintptr_t)g_nvme.regs + 0x1000);
                        g_nvme.regs->cc &= ~1; while(g_nvme.regs->csts & 1);
                        g_nvme.asq = (nvme_sqe_t*)(get_hhdm_offset() + (uintptr_t)pmm_alloc(1));
                        g_nvme.acq = (nvme_cqe_t*)(get_hhdm_offset() + (uintptr_t)pmm_alloc(1));
                        local_memset(g_nvme.asq, 0, 4096); local_memset(g_nvme.acq, 0, 4096);
                        g_nvme.acq_phase = 1; g_nvme.cq_phase = 1;
                        g_nvme.regs->asq = vmm_get_phys(g_nvme.asq); g_nvme.regs->acq = vmm_get_phys(g_nvme.acq);
                        g_nvme.regs->aqa = (63 << 16) | 63; g_nvme.regs->cc = (4 << 20) | (6 << 16) | 1;
                        while(!(g_nvme.regs->csts & 1));
                        g_nvme.sq = (nvme_sqe_t*)(get_hhdm_offset() + (uintptr_t)pmm_alloc(1));
                        g_nvme.cq = (nvme_cqe_t*)(get_hhdm_offset() + (uintptr_t)pmm_alloc(1));
                        local_memset(g_nvme.sq, 0, 4096); local_memset(g_nvme.cq, 0, 4096);
                        nvme_sqe_t create_cq = { .cdw0 = 0x05, .prp1 = vmm_get_phys(g_nvme.cq), .cdw10 = (63 << 16) | 1, .cdw11 = 1 };
                        nvme_cqe_t res; nvme_submit_admin_cmd(&create_cq, &res);
                        nvme_sqe_t create_sq = { .cdw0 = 0x01, .prp1 = vmm_get_phys(g_nvme.sq), .cdw10 = (63 << 16) | 1, .cdw11 = (1 << 16) | 1 };
                        nvme_submit_admin_cmd(&create_sq, &res);
                        vdisk_node_t node = { .name = "NVME_SSD", .sector_size = 512, .read_lba = nvme_read };
                        register_hardware_disk(node);
                    }
                }
            }
        }
    }
}
