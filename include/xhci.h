#ifndef XHCI_H
#define XHCI_H

#include <stdint.h>

/* XHCI Capability Registers */
#define XHCI_CAP_CAPLENGTH  0x00
#define XHCI_CAP_HCIVERSION 0x02
#define XHCI_CAP_HCSPARAMS1 0x04
#define XHCI_CAP_HCSPARAMS2 0x08
#define XHCI_CAP_HCSPARAMS3 0x0C
#define XHCI_CAP_HCCPARAMS1 0x10
#define XHCI_CAP_DBOFF      0x14
#define XHCI_CAP_RTSOFF      0x18
#define XHCI_CAP_HCCPARAMS2 0x1C

/* XHCI Operational Registers (Relative to Base + CapLength) */
#define XHCI_OP_USBCMD      0x00
#define XHCI_OP_USBSTS      0x04
#define XHCI_OP_PAGESIZE    0x08
#define XHCI_OP_DNCTRL      0x14
#define XHCI_OP_CRCR        0x18
#define XHCI_OP_DCBAAP      0x30
#define XHCI_OP_CONFIG      0x38

/* XHCI Runtime Registers (Relative to Base + RTSOFF) */
#define XHCI_RT_IMAN(n)     (0x20 + (n) * 32)
#define XHCI_RT_IMOD(n)     (0x24 + (n) * 32)
#define XHCI_RT_ERSTSZ(n)   (0x28 + (n) * 32)
#define XHCI_RT_ERSTBA(n)   (0x30 + (n) * 32)
#define XHCI_RT_ERDP(n)     (0x38 + (n) * 32)

typedef struct {
    uint64_t ptr;
    uint32_t status;
    uint32_t control;
} __attribute__((packed)) xhci_trb_t;

typedef struct {
    uint64_t ptr;
    uint32_t size;
    uint32_t rsvd;
} __attribute__((packed)) xhci_erst_entry_t;

/* XHCI TRB Types */
#define TRB_TYPE_NORMAL         1
#define TRB_TYPE_SETUP_STAGE    2
#define TRB_TYPE_DATA_STAGE     3
#define TRB_TYPE_STATUS_STAGE   4
#define TRB_TYPE_ENABLE_SLOT    9
#define TRB_TYPE_DISABLE_SLOT   10
#define TRB_TYPE_ADDRESS_DEVICE 11
#define TRB_TYPE_CONFIG_EP      12
#define TRB_TYPE_EVAL_CONTEXT   13
#define TRB_TYPE_RESET_EP       14
#define TRB_TYPE_TRANSFER_EV    32
#define TRB_TYPE_CMD_COMP_EV    33
#define TRB_TYPE_PORT_STATUS_EV 34

/* XHCI Device Context Structures */
typedef struct {
    uint32_t info[2];
    uint32_t reserved[30];
} __attribute__((packed)) xhci_slot_ctx_t;

typedef struct {
    uint32_t info[2];
    uint64_t tr_ptr;
    uint32_t reserved[28];
} __attribute__((packed)) xhci_ep_ctx_t;

typedef struct {
    xhci_slot_ctx_t slot;
    xhci_ep_ctx_t   ep[31];
} __attribute__((packed)) xhci_dev_ctx_t;

typedef struct {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t reserved[6];
    xhci_slot_ctx_t slot;
    xhci_ep_ctx_t   ep[31];
} __attribute__((packed)) xhci_input_ctx_t;

#endif
