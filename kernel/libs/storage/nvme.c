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
    uint32_t cmbloc;
    uint32_t cmbsz;
    uint32_t bpinfo;
    uint32_t bprsel;
    uint32_t bpmbl;
    uint64_t rsv1;
    uint32_t doorbells[];
} nvme_regs_t;

typedef struct { uint8_t opcode; uint8_t flags; uint16_t cid; uint32_t nsid; uint64_t rsv0; uint64_t metadata; uint64_t prp1; uint64_t prp2; uint32_t cdw10[6]; } nvme_cmd_t;
typedef struct { uint32_t cdw0; uint32_t rsv0; uint16_t sq_head; uint16_t sq_id; uint16_t cid; uint16_t status; } nvme_cqe_t;

static nvme_regs_t* nvme_base = NULL;
static void* admin_sq_virt = NULL;
static void* admin_cq_virt = NULL;
static uint16_t admin_sq_tail = 0;
static uint16_t admin_cq_head = 0;
static uint8_t admin_phase = 1;

void nvme_reset_controller(nvme_regs_t* regs) { regs->cc &= ~(1 << 0); }

void pit_wait_ms(uint32_t ms);
static int nvme_wait_ready(nvme_regs_t* regs, int expected) { int timeout = 1000; while (((regs->csts >> 0) & 1) != expected && timeout--) pit_wait_ms(1); return timeout > 0 ? 0 : -1; }

void nvme_enable_controller(nvme_regs_t* regs) { regs->cc |= (1 << 0); }

static void nvme_submit_admin_cmd(nvme_cmd_t* cmd) { nvme_cmd_t* sq = (nvme_cmd_t*)admin_sq_virt; sq[admin_sq_tail] = *cmd; admin_sq_tail = (admin_sq_tail + 1) % 64; nvme_base->doorbells[0] = admin_sq_tail; }

static int nvme_poll_completion(void) { nvme_cqe_t* cq = (nvme_cqe_t*)admin_cq_virt; int timeout = 1000000; while (((cq[admin_cq_head].status >> 0) & 1) != admin_phase && timeout--) __asm__ volatile ("pause"); if (timeout <= 0) return -1; admin_cq_head = (admin_cq_head + 1) % 64; if (admin_cq_head == 0) admin_phase = !admin_phase; nvme_base->doorbells[1] = admin_cq_head; return 0; }

int nvme_identify(void* buffer) { nvme_cmd_t cmd = {0}; cmd.opcode = 0x06; cmd.cid = 0x01; cmd.prp1 = vmm_get_phys(buffer); cmd.cdw10[0] = 1; nvme_submit_admin_cmd(&cmd); return nvme_poll_completion(); }

int nvme_create_io_queues(void* cq_phys, void* sq_phys) { nvme_cmd_t cq_cmd = {0}; cq_cmd.opcode = 0x05; cq_cmd.prp1 = (uint64_t)cq_phys; cq_cmd.cdw10[0] = (63 << 16) | 1; cq_cmd.cdw10[1] = 1; nvme_submit_admin_cmd(&cq_cmd); if (nvme_poll_completion() != 0) return -1; nvme_cmd_t sq_cmd = {0}; sq_cmd.opcode = 0x01; sq_cmd.prp1 = (uint64_t)sq_phys; sq_cmd.cdw10[0] = (63 << 16) | 1; sq_cmd.cdw10[1] = (1 << 16) | 1; nvme_submit_admin_cmd(&sq_cmd); return nvme_poll_completion(); }

static void nvme_setup_prps(nvme_cmd_t* cmd, void* buffer, size_t size) { uint64_t phys = vmm_get_phys(buffer); cmd->prp1 = phys; if (size > 4096) cmd->prp2 = vmm_get_phys((uint8_t*)buffer + 4096); }

static int fault_count = 0;
int nvme_read_lba(uint64_t lba, uint32_t count, void* buffer) { if (fault_count > 0 && --fault_count == 0) return -1; nvme_cmd_t cmd = {0}; cmd.opcode = 0x02; cmd.nsid = 1; nvme_setup_prps(&cmd, buffer, count * 512); cmd.cdw10[0] = (uint32_t)lba; cmd.cdw10[1] = (uint32_t)(lba >> 32); cmd.cdw10[2] = (count - 1) & 0xFFFF; nvme_submit_admin_cmd(&cmd); return nvme_poll_completion(); }
int nvme_write_lba(uint64_t lba, uint32_t count, void* buffer) { nvme_cmd_t cmd = {0}; cmd.opcode = 0x01; cmd.nsid = 1; nvme_setup_prps(&cmd, buffer, count * 512); cmd.cdw10[0] = (uint32_t)lba; cmd.cdw10[1] = (uint32_t)(lba >> 32); cmd.cdw10[2] = (count - 1) & 0xFFFF; nvme_submit_admin_cmd(&cmd); return nvme_poll_completion(); }

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
                        vga_print("[NVME] Base Address: 0x%x\n", (uint64_t)nvme_base);

                        void* slab_alloc_aligned(int id, size_t size, size_t align);
                        admin_sq_virt = slab_alloc_aligned(0, 4096, 4096);
                        admin_cq_virt = slab_alloc_aligned(0, 4096, 4096);

                        nvme_base->aqa = (63 << 16) | 63;
                        uint64_t sq_phys = vmm_get_phys(admin_sq_virt);
                        uint64_t cq_phys = vmm_get_phys(admin_cq_virt);
                        nvme_base->asq = sq_phys;
                        nvme_base->acq = cq_phys;
                        return;
                    }
                }
            }
        }
    }
}
