#ifndef XHCI_H
#define XHCI_H
#include <stdint.h>
#include <stdbool.h>
#define USB_STS_EINT (1 << 3)
#define USB_STS_HCH (1 << 0)
#define USB_STS_CNR (1 << 11)
#define USB_CMD_RS (1 << 0)
#define USB_CMD_HCRST (1 << 1)
#define USB_CMD_INTE (1 << 2)
#define XHCI_INT_ERDP_BUSY (1 << 3)
#define XHCI_TRB_SIZE 16

typedef struct {
    volatile uint8_t capLength;
    uint8_t reserved;
    volatile uint16_t hciVersion;
    volatile uint32_t hcsParams1;
    volatile uint32_t hcsParams2;
    volatile uint32_t hcsParams3;
    union {
        volatile uint32_t hccParams1;
        struct {
            uint32_t addressCap64 : 1;
            uint32_t bwNegotationCap : 1;
            uint32_t contextSize : 1;
            uint32_t portPowerControl : 1;
            uint32_t portIndicators : 1;
            uint32_t lightHCResetCap : 1;
            uint32_t latencyToleranceMessagingCapability : 1;
            uint32_t noSecondarySIDSupport : 1;
            uint32_t parseAllEventData : 1;
            uint32_t shortPacketCapability : 1;
            uint32_t stoppedEDTLACap : 1;
            uint32_t contiguousFrameIDCap : 1;
            uint32_t maxPSASize : 4;
            uint32_t extendedCapPointer : 16;
        } __attribute__((packed));
    };
    volatile uint32_t dbOff;
    volatile uint32_t rtsOff;
} __attribute__((packed)) xhci_cap_regs_t;

typedef struct {
    volatile uint32_t usbCommand;
    volatile uint32_t usbStatus;
    volatile uint32_t pageSize;
    uint8_t rsvd1[8];
    volatile uint32_t deviceNotificationControl;
    uint64_t cmdRingCtl;
    uint8_t rsvd2[16];
    volatile uint64_t devContextBaseAddrArrayPtr;
    volatile uint32_t configure;
} __attribute__((packed)) xhci_op_regs_t;

typedef struct {
    volatile uint32_t portSC;
    volatile uint32_t portPMSC;
    volatile uint32_t portLinkInfo;
    volatile uint32_t portHardwareLPMCtl;
} __attribute__((packed)) xhci_port_regs_t;

typedef struct {
    volatile uint32_t interruptPending : 1;
    volatile uint32_t interruptEnable : 1;
    uint32_t rsvdP : 30;
    uint32_t intModeration;
    uint32_t erstSize;
    uint32_t rsvdP2;
    uint64_t erstBase;
    uint64_t erDequeuePtr;
} __attribute__((packed)) xhci_interrupter_t;

typedef struct {
    uint64_t ringSegmentBaseAddress;
    uint32_t ringSegmentSize : 16;
    uint32_t rsvdZ2 : 16;
    uint32_t rsvd3;
} __attribute__((packed)) xhci_event_ring_segment_table_entry_t;

typedef struct {
    uint32_t microframeIndex;
    uint32_t rsvdZ[7];
    xhci_interrupter_t interrupters[];
} __attribute__((packed)) xhci_runtime_regs_t;

typedef struct {
    volatile uint32_t doorbell;
} __attribute__((packed)) xhci_doorbell_register_t;

typedef struct {
    uint32_t parameter[2];
    uint32_t status;
    uint32_t cycleBit : 1;
    uint32_t extra : 9;
    uint32_t trbType : 6;
    uint32_t control : 16;
} __attribute__((packed)) xhci_trb_t;

typedef struct {
    uint64_t segmentPtr;
    uint32_t rsvdZ : 24;
    uint32_t interrupterTarget : 8;
    uint32_t cycleBit : 1;
    uint32_t toggleCycle : 1;
    uint32_t rsvdZ2 : 2;
    uint32_t chainBit : 1;
    uint32_t interruptOnCompletion : 1;
    uint32_t rsvdZ3 : 4;
    uint32_t trbType : 6;
    uint32_t rsvdZ4 : 16;
} __attribute__((packed)) xhci_link_trb_t;

typedef struct {
    uint64_t ptr;
    uint32_t rsvdZ : 24;
    uint32_t completionCode : 8;
    uint32_t cycleBit : 1;
    uint32_t rsvdZ2 : 9;
    uint32_t trbType : 6;
    uint32_t rsvdZ3 : 16;
} __attribute__((packed)) xhci_event_trb_t;

typedef struct {
    uint64_t pointer;
    uint32_t transferLength : 24;
    uint32_t completionCode : 8;
    uint32_t cycleBit : 1;
    uint32_t rsvdZ : 1;
    uint32_t eventData : 1;
    uint32_t rsvdZ2 : 7;
    uint32_t trbType : 6;
    uint32_t endpointID : 5;
    uint32_t rsvdZ3 : 3;
    uint32_t slotID : 8;
} __attribute__((packed)) xhci_transfer_event_trb_t;

typedef struct {
    uintptr_t phys;
    xhci_trb_t* virt;
    uint32_t enqueue;
    uint32_t max;
    bool cycle;
} xhci_ring_t;

typedef struct {
    uintptr_t base;
    uintptr_t mmio;
    xhci_cap_regs_t* cap;
    xhci_op_regs_t* op;
    xhci_port_regs_t* ports;
    xhci_runtime_regs_t* run;
    xhci_doorbell_register_t* db;
    xhci_ring_t cmd;
    struct {
        uintptr_t seg_table_phys;
        xhci_event_ring_segment_table_entry_t* seg_table;
        uintptr_t ring_phys;
        xhci_event_trb_t* ring;
        uint32_t dequeue;
        bool cycle;
    } ev;
    uint32_t lock;
} xhci_controller_t;

typedef struct {
    uint8_t slot_id;
} xhci_device_t;
#endif
