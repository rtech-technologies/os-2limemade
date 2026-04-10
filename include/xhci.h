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

#endif
