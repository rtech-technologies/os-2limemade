#include <kernel/libs/core/services.h>
#include <include/nvme.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>

void* arc_alloc(size_t size);
uint64_t vmm_get_phys(void* virt);
uint64_t get_hhdm_offset(void);
void vga_print(const char* fmt, ...);
void pit_wait_ms(uint32_t ms);

#define QUEUE_SIZE 64

typedef struct {
    uint8_t bus, slot, func;
    uintptr_t bar;
    nvme_cmd_t* asq;
    nvme_cqe_t* acq;
    uint16_t asq_tail;
    uint16_t acq_head;
    uint16_t acq_phase;
    uint32_t db_stride;

    /* I/O Queues */
    nvme_cmd_t* sq;
    nvme_cqe_t* cq;
    uint16_t sq_tail;
    uint16_t cq_head;
    uint16_t cq_phase;
} nvme_ctrl_t;

static nvme_ctrl_t* g_nvme = NULL;

static void nvme_write_doorbell(nvme_ctrl_t* ctrl, int qid, int tail, bool is_cq) {
    uint32_t offset = 0x1000 + ((2 * qid + (is_cq ? 1 : 0)) * ctrl->db_stride);
    __asm__ volatile ("mfence" ::: "memory");
    *(volatile uint32_t*)(ctrl->bar + offset) = tail;
}

static int nvme_submit_cmd(nvme_ctrl_t* ctrl, int qid, nvme_cmd_t* cmd, nvme_cqe_t* cqe) {
    nvme_cmd_t* sq = (qid == 0) ? ctrl->asq : ctrl->sq;
    nvme_cqe_t* cq = (qid == 0) ? ctrl->acq : ctrl->cq;
    uint16_t* sq_tail = (qid == 0) ? &ctrl->asq_tail : &ctrl->sq_tail;
    uint16_t* cq_head = (qid == 0) ? &ctrl->acq_head : &ctrl->cq_head;
    uint16_t* cq_phase = (qid == 0) ? &ctrl->acq_phase : &ctrl->cq_phase;

    sq[*sq_tail] = *cmd;
    *sq_tail = (*sq_tail + 1) % QUEUE_SIZE;
    nvme_write_doorbell(ctrl, qid, *sq_tail, false);

    int timeout = 1000;
    while (timeout--) {
        nvme_cqe_t* check = &cq[*cq_head];
        if ((check->status & 1) == *cq_phase) {
            if (cqe) *cqe = *check;
            *cq_head = (*cq_head + 1) % QUEUE_SIZE;
            if (*cq_head == 0) *cq_phase = !(*cq_phase);
            nvme_write_doorbell(ctrl, qid, *cq_head, true);
            return 0;
        }
        pit_wait_ms(1);
    }
    return -1;
}

static int nvme_submit_admin(nvme_ctrl_t* ctrl, nvme_cmd_t* cmd, nvme_cqe_t* cqe) {
    return nvme_submit_cmd(ctrl, 0, cmd, cqe);
}

void nvme_init_ctrl(uint8_t bus, uint8_t slot, uint8_t func) {
    if (g_nvme) return;
    vga_print("[NVME] Initializing Controller at %d:%d:%d\n", bus, slot, func);
    pci_enable_master(bus, slot, func);

    uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
    uint32_t bar1 = pci_config_read(bus, slot, func, 0x14);
    uint64_t phys_base = ((uint64_t)bar1 << 32) | (bar0 & 0xFFFFFFF0);
    uintptr_t virt_base = get_hhdm_offset() + phys_base;

    g_nvme = arc_alloc(sizeof(nvme_ctrl_t));
    g_nvme->bus = bus; g_nvme->slot = slot; g_nvme->func = func;
    g_nvme->bar = virt_base;

    /* Read Capabilities */
    uint64_t cap = *(volatile uint64_t*)(virt_base + NVME_REG_CAP);
    g_nvme->db_stride = 4 << ((cap >> 32) & 0xF);

    /* Reset Controller */
    *(volatile uint32_t*)(virt_base + NVME_REG_CC) &= ~(1 << 0);
    pit_wait_ms(100);

    /* Allocate Admin Queues */
    g_nvme->asq = arc_alloc(4096);
    g_nvme->acq = arc_alloc(4096);
    g_nvme->asq_tail = 0;
    g_nvme->acq_head = 0;
    g_nvme->acq_phase = 1;

    /* Configure Admin Queues in AQA */
    *(volatile uint32_t*)(virt_base + NVME_REG_AQA) = (QUEUE_SIZE - 1) | ((QUEUE_SIZE - 1) << 16);
    *(volatile uint64_t*)(virt_base + NVME_REG_ASQ) = vmm_get_phys(g_nvme->asq);
    *(volatile uint64_t*)(virt_base + NVME_REG_ACQ) = vmm_get_phys(g_nvme->acq);

    /* Enable Controller: CC.MPS=0 (4KB), CC.CSS=0 (NVM), CC.AMS=0 (Round Robin), CC.EN=1 */
    /* IOSQES=6 (64 bytes), IOCQES=4 (16 bytes) */
    uint32_t cc = (6 << 16) | (4 << 20) | (1 << 0);
    *(volatile uint32_t*)(virt_base + NVME_REG_CC) = cc;

    pit_wait_ms(100);
    if (!(*(volatile uint32_t*)(virt_base + NVME_REG_CSTS) & (1 << 0))) {
        vga_print("[NVME] FATAL: Controller failed to enable.\n");
        return;
    }

    /* Create I/O Completion Queue */
    g_nvme->cq = arc_alloc(4096);
    g_nvme->cq_head = 0; g_nvme->cq_phase = 1;
    nvme_cmd_t create_cq = {
        .cdw0 = NVME_ADMIN_OP_CREATE_CQ,
        .prp1 = vmm_get_phys(g_nvme->cq),
        .cdw10 = (QUEUE_SIZE - 1) << 16 | 1, /* QID=1, Size=64 */
        .cdw11 = 1 /* PC: Physically Contiguous */
    };
    nvme_cqe_t res;
    if (nvme_submit_admin(g_nvme, &create_cq, &res) != 0) return;

    /* Create I/O Submission Queue */
    g_nvme->sq = arc_alloc(4096);
    g_nvme->sq_tail = 0;
    nvme_cmd_t create_sq = {
        .cdw0 = NVME_ADMIN_OP_CREATE_SQ,
        .prp1 = vmm_get_phys(g_nvme->sq),
        .cdw10 = (QUEUE_SIZE - 1) << 16 | 1, /* QID=1, Size=64 */
        .cdw11 = 1 << 16 | 1 /* CQID=1, PC=1 */
    };
    if (nvme_submit_admin(g_nvme, &create_sq, &res) != 0) return;

    vga_print("[SNAP] NVME I/O QUEUES INITIALIZED\n");

    /* Register NVMe as a disk */
    extern int nvme_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer);
    extern int nvme_write_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer);

    vdisk_node_t nvme_disk = {
        .name = "NVME_SSD",
        .sector_size = 512,
        .total_lba = 1024 * 1024 * 20,
        .partition_offset = 2048,
        .read_lba = nvme_read_sectors,
        .write_lba = nvme_write_sectors,
        .private_data = g_nvme,
        .is_atapi = false
    };
    register_hardware_disk(nvme_disk);
}

int nvme_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    nvme_ctrl_t* ctrl = (nvme_ctrl_t*)priv;
    uint64_t phys_buffer = vmm_get_phys(buffer);

    nvme_cmd_t cmd = {0};
    cmd.cdw0 = NVME_NVM_OP_READ | (1 << 16); /* Opcode 2, CID 1 */
    cmd.nsid = 1;
    cmd.prp1 = phys_buffer;

    /* PRP2 Handling for > 1 page */
    if (count * 512 > 4096) {
        cmd.prp2 = phys_buffer + 4096;
    }

    cmd.cdw10 = (uint32_t)lba;
    cmd.cdw11 = (uint32_t)(lba >> 32);
    cmd.cdw12 = (count - 1) & 0xFFFF;

    return nvme_submit_cmd(ctrl, 1, &cmd, NULL);
}

int nvme_write_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    nvme_ctrl_t* ctrl = (nvme_ctrl_t*)priv;
    uint64_t phys_buffer = vmm_get_phys(buffer);

    nvme_cmd_t cmd = {0};
    cmd.cdw0 = NVME_NVM_OP_WRITE | (2 << 16); /* Opcode 1, CID 2 */
    cmd.nsid = 1;
    cmd.prp1 = phys_buffer;

    if (count * 512 > 4096) {
        cmd.prp2 = phys_buffer + 4096;
    }

    cmd.cdw10 = (uint32_t)lba;
    cmd.cdw11 = (uint32_t)(lba >> 32);
    cmd.cdw12 = (count - 1) & 0xFFFF;

    return nvme_submit_cmd(ctrl, 1, &cmd, NULL);
}

void nvme_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        pci_driver_t nvme_driver = {
            .vendor_id = 0xFFFF, .device_id = 0xFFFF,
            .class_code = 0x01, .subclass_code = 0x08, .prog_if = 0x02,
            .init = nvme_init_ctrl
        };
        pci_register_driver(nvme_driver);
    }
}
