#ifndef NVME_H
#define NVME_H

#include <stdint.h>

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

/* Admin Command Opcode */
#define NVME_ADMIN_OP_IDENTIFY 0x06
#define NVME_ADMIN_OP_CREATE_SQ 0x01
#define NVME_ADMIN_OP_CREATE_CQ 0x05

/* NVM Command Opcode */
#define NVME_NVM_OP_READ  0x02
#define NVME_NVM_OP_WRITE 0x01

typedef struct {
    uint32_t cdw0;      /* Opcode, CID, etc */
    uint32_t nsid;
    uint32_t rsvd2[2];
    uint64_t metadata;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} nvme_cmd_t;

typedef struct {
    uint32_t cdw0;
    uint32_t rsvd;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t command_id;
    uint16_t status;
} nvme_cqe_t;

#endif
