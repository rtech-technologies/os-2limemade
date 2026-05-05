#include <kernel/libs/core/services.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <kernel/libs/storage/vdisk.h>
#include <kernel/libs/core/pci.h>

void vga_print(const char* fmt, ...);
void* bump_alloc(size_t size);
void* slab_alloc_aligned(int id, size_t size, size_t align);
uint64_t vmm_get_phys(void* virt);
uint64_t get_hhdm_offset(void);

/* NVMe Register Offsets */
#define NVME_REG_CAP     0x00
#define NVME_REG_VS      0x08
#define NVME_REG_INTMS   0x0C
#define NVME_REG_INTMC   0x10
#define NVME_REG_CC      0x14
#define NVME_REG_CSTS    0x1C
#define NVME_REG_NSSR    0x20
#define NVME_REG_AQA     0x24
#define NVME_REG_ASQ     0x28
#define NVME_REG_ACQ     0x30

/* NVMe Controller Structure (Simplified) */
typedef struct {
    volatile uint64_t cap;
    volatile uint32_t vs;
    volatile uint32_t intms;
    volatile uint32_t intmc;
    volatile uint32_t cc;
    volatile uint32_t rsv0;
    volatile uint32_t csts;
    volatile uint32_t nssr;
    volatile uint32_t aqa;
    volatile uint64_t asq;
    volatile uint64_t acq;
} nvme_regs_t;

typedef struct {
    uint32_t cdw0;
    uint32_t nsid;
    uint32_t rsvd[2];
    uint64_t mptr;
    uint64_t dptr[2];
    uint32_t cdw10[6];
} nvme_command_t;

typedef struct {
    uint32_t cdw0;
    uint32_t rsvd;
    uint16_t sqhd;
    uint16_t sqid;
    uint16_t cid;
    uint16_t status;
} nvme_completion_t;

typedef struct {
    nvme_command_t* cmds;
    uint16_t size;
    uint16_t tail;
    uint32_t* doorbell;
} nvme_sq_t;

typedef struct {
    nvme_completion_t* cpls;
    uint16_t size;
    uint16_t head;
    uint16_t phase;
    uint32_t* doorbell;
} nvme_cq_t;

static nvme_regs_t* nvme_base = NULL;
static nvme_sq_t admin_sq;
static nvme_cq_t admin_cq;
static nvme_sq_t io_sq;
static nvme_cq_t io_cq;
static uint32_t doorbell_stride;
static uint64_t ns_size = 0;

static uint16_t nvme_submit_command(nvme_sq_t* sq, nvme_cq_t* cq, nvme_command_t cmd);

static int nvme_wait_csts_ready(bool ready, int timeout_ms) {
    void pit_wait_ms(uint32_t ms);
    for (int i = 0; i < timeout_ms; i++) {
        if (((nvme_base->csts & 1) != 0) == ready) return 0;
        pit_wait_ms(1);
    }
    return -1;
}

static void nvme_probe(uint8_t bus, uint8_t slot, uint8_t func, pci_id_t id) {
    if (nvme_base != NULL) return;
    (void)id;
    vga_print("[NVME] Found NVMe Controller at %d:%d:%d\n", bus, slot, func);

    uint32_t bar0 = pci_config_read(bus, slot, func, 0x10);
    uint32_t bar1 = pci_config_read(bus, slot, func, 0x14);
    uint64_t phys_base = ((uint64_t)bar1 << 32) | (bar0 & 0xFFFFFFF0);

    nvme_base = (nvme_regs_t*)(get_hhdm_offset() + phys_base);
    vga_print("[NVME] Base Address: 0x%x\n", (uint64_t)nvme_base);

    /* 1. Controller Reset */
    if (nvme_base->cc & (1 << 0)) {
        nvme_base->cc &= ~(1 << 0);
    }
    nvme_wait_csts_ready(false, 1000);

    /* 2. Setup Admin Queues */
    doorbell_stride = 4 << ((nvme_base->cap >> 32) & 0xF);

    admin_sq.size = 64;
    admin_cq.size = 64;
    admin_sq.cmds = slab_alloc_aligned(0, admin_sq.size * sizeof(nvme_command_t), 4096);
    admin_cq.cpls = slab_alloc_aligned(0, admin_cq.size * sizeof(nvme_completion_t), 4096);

    nvme_base->aqa = ((admin_cq.size - 1) << 16) | (admin_sq.size - 1);
    nvme_base->asq = vmm_get_phys(admin_sq.cmds);
    nvme_base->acq = vmm_get_phys(admin_cq.cpls);

    admin_sq.doorbell = (uint32_t*)((uint8_t*)nvme_base + 0x1000 + (0 * 2 * doorbell_stride));
    admin_cq.doorbell = (uint32_t*)((uint8_t*)nvme_base + 0x1000 + (0 * 2 * doorbell_stride) + doorbell_stride);

    admin_sq.tail = 0;
    admin_cq.head = 0;
    admin_cq.phase = 1;

    /* 3. Enable Controller */
    nvme_base->cc = (1 << 0) | (0 << 11) | (0 << 14) | (4 << 16) | (6 << 20);
    if (nvme_wait_csts_ready(true, 1000) != 0) {
        vga_print("[NVME] Error: Controller ready timeout.\n");
        return;
    }

    vga_print("[NVME] Admin Queues Initialized.\n");

    /* 4. Create I/O Completion Queue */
    io_cq.size = 256;
    io_cq.cpls = slab_alloc_aligned(0, io_cq.size * sizeof(nvme_completion_t), 4096);
    io_cq.head = 0;
    io_cq.phase = 1;
    io_cq.doorbell = (uint32_t*)((uint8_t*)nvme_base + 0x1000 + (1 * 2 * doorbell_stride) + doorbell_stride);

    nvme_command_t create_cq = {0};
    create_cq.cdw0 = 0x05; /* Create I/O Completion Queue */
    create_cq.dptr[0] = vmm_get_phys(io_cq.cpls);
    create_cq.cdw10[0] = ((io_cq.size - 1) << 16) | 0x01; /* QID 1 */
    create_cq.cdw10[1] = 0x01; /* Physically Contiguous */
    if (nvme_submit_command(&admin_sq, &admin_cq, create_cq) != 0) {
        vga_print("[NVME] Error: Failed to create I/O CQ.\n");
        return;
    }

    /* 5. Create I/O Submission Queue */
    io_sq.size = 256;
    io_sq.cmds = slab_alloc_aligned(0, io_sq.size * sizeof(nvme_command_t), 4096);
    io_sq.tail = 0;
    io_sq.doorbell = (uint32_t*)((uint8_t*)nvme_base + 0x1000 + (1 * 2 * doorbell_stride));

    nvme_command_t create_sq = {0};
    create_sq.cdw0 = 0x01; /* Create I/O Submission Queue */
    create_sq.dptr[0] = vmm_get_phys(io_sq.cmds);
    create_sq.cdw10[0] = ((io_sq.size - 1) << 16) | 0x01; /* QID 1 */
    create_sq.cdw10[1] = (0x01 << 16) | 0x01; /* CQID 1, Physically Contiguous */
    if (nvme_submit_command(&admin_sq, &admin_cq, create_sq) != 0) {
        vga_print("[NVME] Error: Failed to create I/O SQ.\n");
        return;
    }

    /* 6. Identify Namespace */
    uint8_t* id_data = slab_alloc_aligned(0, 4096, 4096);
    nvme_command_t identify = {0};
    identify.cdw0 = 0x06;
    identify.nsid = 1;
    identify.dptr[0] = vmm_get_phys(id_data);
    identify.cdw10[0] = 0; /* Identify Namespace */
    if (nvme_submit_command(&admin_sq, &admin_cq, identify) == 0) {
        ns_size = *(uint64_t*)id_data;
        vga_print("[NVME] Namespace 1: %d sectors.\n", ns_size);

        vdisk_node_t nvme_disk = {
            .name = "NVME_DISK",
            .sector_size = 512,
            .total_lba = ns_size,
            .partition_offset = 2048,
            .read_lba = NULL, /* TODO: Implement */
            .write_lba = NULL,
            .private_data = NULL,
            .is_atapi = false
        };
        void register_hardware_disk(vdisk_node_t node);
        register_hardware_disk(nvme_disk);
    }
}

int nvme_read_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    (void)priv;
    nvme_command_t cmd = {0};
    cmd.cdw0 = 0x02; /* Read */
    cmd.nsid = 1;
    cmd.dptr[0] = vmm_get_phys(buffer);
    cmd.cdw10[0] = (uint32_t)lba;
    cmd.cdw10[1] = (uint32_t)(lba >> 32);
    cmd.cdw10[2] = (count - 1) & 0xFFFF;

    if (nvme_submit_command(&io_sq, &io_cq, cmd) == 0) return 0;
    return -1;
}

int nvme_write_sectors(void* priv, uint64_t lba, uint32_t count, void* buffer) {
    (void)priv;
    nvme_command_t cmd = {0};
    cmd.cdw0 = 0x01; /* Write */
    cmd.nsid = 1;
    cmd.dptr[0] = vmm_get_phys(buffer);
    cmd.cdw10[0] = (uint32_t)lba;
    cmd.cdw10[1] = (uint32_t)(lba >> 32);
    cmd.cdw10[2] = (count - 1) & 0xFFFF;

    if (nvme_submit_command(&io_sq, &io_cq, cmd) == 0) return 0;
    return -1;
}

static uint16_t nvme_submit_command(nvme_sq_t* sq, nvme_cq_t* cq, nvme_command_t cmd) {
    sq->cmds[sq->tail] = cmd;
    sq->tail = (sq->tail + 1) % sq->size;
    *sq->doorbell = sq->tail;

    /* Wait for completion */
    int timeout = 1000000;
    while (timeout--) {
        volatile nvme_completion_t* cpl = &cq->cpls[cq->head];
        if ((cpl->status & 1) == cq->phase) {
            uint16_t status = cpl->status >> 1;
            cq->head = (cq->head + 1) % cq->size;
            if (cq->head == 0) cq->phase = !cq->phase;
            *cq->doorbell = cq->head;
            return status;
        }
        __asm__ volatile ("pause");
    }
    return 0xFFFF; /* Timeout */
}

static pci_id_t nvme_ids[] = {
    {0xFFFF, 0xFFFF, 0x01, 0x08, 0x02}
};

void nvme_service(kernel_event_t event) {
    if (event == EVENT_INIT) {
        pci_driver_t driver = {
            .name = "NVMe",
            .id_table = nvme_ids,
            .id_count = 1,
            .probe = nvme_probe
        };
        pci_register_driver(driver);
    }
}
