#ifndef XHCI_H
#define XHCI_H

#include <stdint.h>
#include <stdbool.h>

#define XHCI_PORT_OFFSET 0x400
#define USB_CMD_RS (1 << 0)
#define USB_CMD_HCRST (1 << 1)
#define USB_CMD_INTE (1 << 2)
#define USB_CMD_HSEE (1 << 3)
#define USB_STS_HCH (1 << 0)
#define USB_STS_HSE (1 << 2)
#define USB_STS_EINT (1 << 3)
#define USB_STS_PCD (1 << 4)
#define USB_STS_CNR (1 << 11)
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
    volatile uint32_t hccParams2;
} __attribute__((packed)) xhci_cap_regs_t;

typedef struct {
    volatile uint32_t usbCommand;
    volatile uint32_t usbStatus;
    volatile uint32_t pageSize;
    uint8_t rsvd1[8];
    volatile uint32_t deviceNotificationControl;
    union {
        volatile uint64_t cmdRingCtl;
        struct {
            uint64_t cmdRingCtlRCS : 1;
            uint64_t cmdRingCtlCS : 1;
            uint64_t cmdRingCtlCA : 1;
            uint64_t cmdRingCtlCRR : 1;
            uint64_t cmdRingCtlReserved : 2;
            uint64_t cmdRingCtlPointer : 58;
        } __attribute__((packed));
    } __attribute__((packed));
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
    uint32_t routeString : 20;
    uint32_t speed : 4;
    uint32_t resvd : 1;
    uint32_t mtt : 1;
    uint32_t hub : 1;
    uint32_t ctxEntries : 5;
    uint32_t maxExitLatency : 16;
    uint32_t rootHubPortNumber : 8;
    uint32_t numberOfPorts : 8;
    uint32_t parentHubSlotID : 8;
    uint32_t parentPortNumber : 8;
    uint32_t ttt : 2;
    uint32_t resvdZ : 4;
    uint32_t interrupterTarget : 10;
    uint32_t usbDeviceAddress : 8;
    uint32_t resvdZ_2 : 19;
    uint32_t slotState : 5;
    uint32_t reserved[4];
} __attribute__((packed)) xhci_slot_ctx_t;

typedef struct {
    uint32_t epState : 3;
    uint32_t rsvdZ : 5;
    uint32_t mult : 2;
    uint32_t maxPStreams : 5;
    uint32_t linearStreamArray : 1;
    uint32_t interval : 8;
    uint32_t maxESITPayloadHigh : 8;
    uint32_t rsvdZ2 : 1;
    uint32_t errorCount : 2;
    uint32_t endpointType : 3;
    uint32_t rsvdZ3 : 1;
    uint32_t hostInitiateDisable : 1;
    uint32_t maxBurstSize : 8;
    uint32_t maxPacketSize : 16;
    uint64_t dequeueCycleState : 1;
    uint64_t rsvdZ4 : 3;
    uint64_t trDequeuePointer : 60;
    uint32_t averageTRBLength : 16;
    uint32_t maxESITPayloadLow : 16;
    uint32_t rsvdO[3];
} __attribute__((packed)) xhci_ep_ctx_t;

typedef struct {
    xhci_slot_ctx_t slot;
    xhci_ep_ctx_t ep0;
    xhci_ep_ctx_t eps[30];
} __attribute__((packed)) xhci_device_ctx_t;

typedef struct {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t reserved[5];
    uint8_t  conf_val;
    uint8_t  intf_num;
    uint8_t  alt_set;
    uint8_t  reserved2;
} __attribute__((packed)) xhci_input_ctrl_ctx_t;

typedef struct {
    xhci_input_ctrl_ctx_t ctrl;
    xhci_device_ctx_t dev;
} __attribute__((packed)) xhci_input_ctx_t;

typedef struct {
    volatile uint32_t interruptPending : 1;
    volatile uint32_t interruptEnable : 1;
    uint32_t rsvdP : 30;
    volatile uint32_t intModerationInterval : 16;
    volatile uint32_t intModerationCounter : 16;
    volatile uint32_t eventRingSegmentTableSize : 16;
    uint32_t rsvdP2 : 16;
    uint32_t reserved;
    volatile uint64_t eventRingSegmentTableBaseAddress;
    union {
        volatile uint64_t eventRingDequeuePointer;
        struct {
            uint32_t dequeueERSTSegmentIndex : 3;
            uint32_t eventHandlerBusy : 1;
            uint32_t eventRingDequeuePointerLow : 28;
            uint32_t eventRingDequeuePointerHigh;
        };
    };
} __attribute__((packed)) xhci_interrupter_t;

typedef struct {
    uint64_t ringSegmentBaseAddress;
    uint32_t ringSegmentSize : 16;
    uint32_t rsvdZ2 : 16;
    uint32_t rsvd3;
} __attribute__((packed)) xhci_event_ring_segment_table_entry_t;

typedef struct {
    volatile uint32_t microframeIndex;
    uint32_t rsvdZ[7];
    xhci_interrupter_t interrupters[];
} __attribute__((packed)) xhci_runtime_regs_t;

typedef union {
    volatile uint32_t doorbell;
    struct {
        uint32_t target : 8;
        uint32_t rsvdZ : 8;
        uint32_t streamID : 16;
    } __attribute__((packed));
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
    uint64_t commandTRBPointer;
    uint32_t commandCompletionParameter : 24;
    uint32_t completionCode : 8;
    uint32_t cycleBit : 1;
    uint32_t rsvdZ : 9;
    uint32_t trbType : 6;
    uint32_t vfID : 8;
    uint32_t slotID : 8;
} __attribute__((packed)) xhci_command_completion_event_trb_t;

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

typedef struct xhci_controller xhci_controller_t;

typedef struct {
    uintptr_t phys;
    xhci_trb_t* virt;
    uint32_t enqueue;
    uint32_t max;
    bool cycle;
} xhci_ring_t;

typedef struct {
    uint8_t slot_id;
    xhci_device_ctx_t* ctx;
    uintptr_t ctx_phys;
    xhci_ring_t rings[32];
    bool is_tablet;
} xhci_device_t;

struct xhci_controller {
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
    uint64_t* dcbaa;
    uintptr_t dcbaa_phys;
    xhci_device_t* devices[256];
    uint32_t lock;
};

#endif
