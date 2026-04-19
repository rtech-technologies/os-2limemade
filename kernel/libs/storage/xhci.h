#ifndef XHCI_H
#define XHCI_H

#include <stdint.h>
#include <stdbool.h>

#define XHCI_PORT_OFFSET 0x400

#define USB_CMD_RS (1 << 0)    // Run/Stop
#define USB_CMD_HCRST (1 << 1) // Host Controller Reset
#define USB_CMD_INTE (1 << 2)  // Interrupter enable
#define USB_CMD_HSEE (1 << 3)  // Host System Error enable

#define USB_STS_HCH (1 << 0)  // HCHalted - 0 if CMD_RS is 1
#define USB_STS_HSE (1 << 2)  // Host System Error - set to 1 on error
#define USB_STS_EINT (1 << 3) // Event Interrupt
#define USB_STS_PCD (1 << 4)  // Port change detect
#define USB_STS_SSS (1 << 8)  // Save State Status - 1 when CMD_CSS is 1
#define USB_STS_RSS (1 << 9)  // Restore State Status - 1 when CMD_CRS is 1
#define USB_STS_SRE (1 << 10) // Save/Restore Error - 1 when error during save or restore operation
#define USB_STS_CNR (1 << 11) // Controller Not Ready - 0 = Ready, 1 = Not Ready
#define USB_STS_HCE (1 << 12) // Host Controller Error

#define USB_CFG_MAXSLOTSEN (0xFF) // Max slots enabled
#define USB_CFG_U3E (1 << 8)      // U3 Entry Enable
#define USB_CFG_CIE (1 << 9)      // Configuration Information Enable

#define USB_CCR_RCS (1 << 0) // Ring Cycle State
#define USB_CCR_CS (1 << 1)  // Command Stop
#define USB_CCR_CA (1 << 2)  // Command Abort
#define USB_CCR_CRR (1 << 3) // Command Ring Running

#define USB_CCR_PTR_LO 0xFFFFFFC0
#define USB_CCR_PTR 0xFFFFFFFFFFFFFFC0 // Command Ring Pointer

#define XHCI_PORTSC_CCS (1 << 0) // Current Connect Status
#define XHCI_PORTSC_PED (1 << 1) // Port Enabled/Disabled
#define XHCI_PORTSC_OCA (1 << 3) // Overcurrent Active
#define XHCI_PORTSC_PR  (1 << 4) // Port Reset
#define XHCI_PORTSC_PP  (1 << 9) // Port Power
#define XHCI_PORTSC_CSC (1 << 17) // Connect Status Change
#define XHCI_PORTSC_PEC (1 << 18) // Port Enabled/Disabled Change
#define XHCI_PORTSC_PRC (1 << 21) // Port Reset Change
#define XHCI_PORTSC_WPR (1 << 31) // On USB3 ports warm reset

#define XHCI_INT_ERDP_BUSY (1 << 3)

#define XHCI_TRB_SIZE 16
#define XHCI_EVENT_RING_SEGMENT_TABLE_ENTRY_SIZE 16

typedef enum {
    XHCIExtCapLegacySupport = 1,
    XHCIExtCapSupportedProtocol = 2,
    XHCIExtCapExtendedPowerManagement = 3,
    XHCIExtCapIOVirtualization = 4,
    XHCIExtCapMessageInterrupt = 5,
    XHCIExtCapLocalMemory = 6,
    XHCIExtCapUSBDebug = 10,
    XHCIExtCapExtendedMessageInterrupt = 17,
} xhci_ext_cap_type_t;

typedef struct {
    volatile uint8_t capLength; // Capability Register Length
    uint8_t reserved;
    volatile uint16_t hciVersion; // Interface Version Number
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
    volatile uint32_t dbOff;  // Doorbell offset
    volatile uint32_t rtsOff; // Runtime registers space offset
    volatile uint32_t hccParams2;
} __attribute__((packed)) xhci_cap_regs_t;

typedef struct {
    volatile uint32_t usbCommand; // USB Command
    volatile uint32_t usbStatus;  // USB Status
    volatile uint32_t pageSize;   // Page Size
    uint8_t rsvd1[8];
    volatile uint32_t deviceNotificationControl; // Device Notification Control
    union {
        volatile uint64_t cmdRingCtl; // Command Ring Control
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
    volatile uint64_t devContextBaseAddrArrayPtr; // Device Context Base Address Array Pointer
    volatile uint32_t configure;                  // Configure
} __attribute__((packed)) xhci_op_regs_t;

typedef struct {
    volatile uint32_t portSC;             // Port Status and Control
    volatile uint32_t portPMSC;           // Power Management Status and Control
    volatile uint32_t portLinkInfo;       // Port Link Info
    volatile uint32_t portHardwareLPMCtl; // Port Hardware LPM Control
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
    uint32_t reserved[4]; // Padding to 32 bytes
} __attribute__((packed)) xhci_slot_context_t;

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
} __attribute__((packed)) xhci_endpoint_context_t;

typedef struct {
    volatile uint32_t interruptPending : 1;
    volatile uint32_t interruptEnable : 1;
    uint32_t rsvdP : 30;
    volatile uint32_t intModerationInterval : 16;
    volatile uint32_t intModerationCounter : 16;
    volatile uint32_t eventRingSegmentTableSize : 16;
    uint32_t rsvdP2 : 16;
    uint32_t reserved; // Hole at 0x0C
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
    union {
        uint64_t ringSegmentBaseAddress;
        struct {
            uint32_t rsvdZ : 6;
            uint32_t ringSegmentBaseAddressLo : 26;
            uint32_t ringSegmentBaseAddressHigh : 32;
        } __attribute__((packed));
    } __attribute__((packed));
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

typedef union {
    struct {
        volatile uint32_t capability;
    };
    struct {
        uint32_t capID : 8;
        uint32_t nextCap : 8;
        uint32_t controllerBIOSSemaphore : 1;
        uint32_t rsvdP : 7;
        uint32_t controllerOSSemaphore : 1;
        uint32_t rsvdP2 : 7;
    } __attribute__((packed));
} __attribute__((packed)) xhci_ext_cap_legacy_support_t;

typedef struct {
    volatile uint32_t capID : 8;
    volatile uint32_t nextCap : 8;
    volatile uint32_t minorRevision : 8;
    volatile uint32_t majorRevision : 8;
    volatile uint8_t name[4];
    volatile uint32_t portOffset : 8;
    volatile uint32_t portCount : 8;
    volatile uint32_t protoSpecific : 12;
    volatile uint32_t speedIDCount : 4;
    volatile uint32_t slotType : 4;
    volatile uint32_t rsvdP : 28;
} __attribute__((packed)) xhci_ext_cap_supported_protocol_t;

typedef struct {
    xhci_endpoint_context_t out;
    xhci_endpoint_context_t in;
} __attribute__((packed)) xhci_endpoint_context_pair_t;

typedef struct {
    xhci_slot_context_t slot;
    xhci_endpoint_context_t controlEndpoint;
    xhci_endpoint_context_pair_t endpoints[15];
} __attribute__((packed)) xhci_device_context_t;

typedef struct {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t reserved[5];
    uint32_t configuration_value : 8;
    uint32_t interface_number : 8;
    uint32_t alternate_setting : 8;
    uint32_t reserved2 : 8;
} __attribute__((packed)) xhci_input_control_ctx_t;

typedef struct {
    xhci_input_control_ctx_t control;
    xhci_device_context_t device;
} __attribute__((packed)) xhci_input_context_t;

typedef enum {
    TRBTypeNormal = 1,
    TRBTypeSetup = 2,
    TRBTypeData = 3,
    TRBTypeStatus = 4,
    TRBTypeIsoch = 5,
    TRBTypeLink = 6,
    TRBTypeEventData = 7,
    TRBTypeNoOp = 8,
    TRBTypeEnableSlotCommand = 9,
    TRBTypeDisableSlotCommand = 10,
    TRBTypeAddressDeviceCommand = 11,
    TRBTypeConfigureEndpointCommand = 12,
    TRBTypeEvaluateContextCommand = 13,
    TRBTypeResetEndpointCommand = 14,
    TRBTypeStopEndpointCommand = 15,
    TRBTypeSetTRDequeuePointerCommand = 16,
    TRBTypeResetDeviceCommand = 17,
    TRBTypeForceEventCommand = 18,
    TRBTypeNegotiateBandwidthCommand = 19,
    TRBTypeSetLatencyZToleranceValueCommand = 20,
    TRBTypeGetPortBandwidthCommand = 21,
    TRBTypeForceHeaderCommand = 22,
    TRBTypeNoOpCommand = 23,
    TRBTypeGetExtendedPropertyCommand = 24,
    TRBTypeSetExtendedPropertyCommand = 25,
    TRBTypeTransferEvent = 32,
    TRBTypeCommandCompletionEvent = 33,
    TRBTypePortStatusChangeEvent = 34,
    TRBTypeBandwidthRequestEvent = 35,
    TRBTypeDoorbellEvent = 36,
    TRBTypeHostControllerEvent = 37,
    TRBTypeDeviceNotificationEvent = 38,
    TRBTypeMFINDEXWrapEvent = 39,
} xhci_trb_type_t;

typedef enum {
    EndpointTypeControl = 4,
} xhci_ep_type_t;

typedef enum {
    NoDataStage = 0,
    OUTDataStage = 2,
    INDataStage = 3,
} xhci_transfer_type_t;

typedef struct {
    uint32_t parameter[2];
    uint32_t status;
    uint32_t cycleBit : 1;
    uint32_t extra : 9;
    uint32_t trbType : 6;
    uint32_t control : 16;
} __attribute__((packed)) xhci_trb_t;

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
    uint64_t dataBuffer;
    uint32_t transferLength : 17;
    uint32_t size : 5;
    uint32_t interrupterTarget : 10;
    uint32_t cycleBit : 1;
    uint32_t evaluateNextTRB : 1;
    uint32_t interruptOnShortPacket : 1;
    uint32_t noSnoop : 1;
    uint32_t chainBit : 1;
    uint32_t interruptOnCompletion : 1;
    uint32_t immediateData : 1;
    uint32_t rsvdZ : 2;
    uint32_t blockEventInterrupt : 1;
    uint32_t trbType : 6;
    uint32_t rsvdZ2 : 16;
} __attribute__((packed)) xhci_normal_trb_t;

typedef struct {
    uint32_t bmRequestType : 8;
    uint32_t bRequest : 8;
    uint32_t wValue : 16;
    uint32_t wIndex : 16;
    uint32_t wLength : 16;
    uint32_t trbTransferLength : 17;
    uint32_t rsvdZ : 5;
    uint32_t interrupterTarget : 10;
    uint32_t cycleBit : 1;
    uint32_t rsvdZ2 : 4;
    uint32_t interruptOnCompletion : 1;
    uint32_t immediateData : 1;
    uint32_t rsvdZ3 : 3;
    uint32_t trbType : 6;
    uint32_t transferType : 2;
    uint32_t rsvdZ4 : 14;
} __attribute__((packed)) xhci_setup_trb_t;

typedef struct {
    uint64_t dataBuffer;
    uint32_t transferLength : 17;
    uint32_t size : 5;
    uint32_t interrupterTarget : 10;
    uint32_t cycleBit : 1;
    uint32_t evaluateNextTRB : 1;
    uint32_t interruptOnShortPacket : 1;
    uint32_t noSnoop : 1;
    uint32_t chainBit : 1;
    uint32_t interruptOnCompletion : 1;
    uint32_t immediateData : 1;
    uint32_t rsvdZ : 3;
    uint32_t trbType : 6;
    uint32_t direction : 1;
    uint32_t rsvdZ2 : 15;
} __attribute__((packed)) xhci_data_trb_t;

typedef struct {
    uint32_t rsvdZ0[2];
    uint32_t rsvdZ : 22;
    uint32_t interrupterTarget : 10;
    uint32_t cycleBit : 1;
    uint32_t evaluateNextTRB : 1;
    uint32_t rsvdZ2 : 2;
    uint32_t chainBit : 1;
    uint32_t interruptOnCompletion : 1;
    uint32_t rsvdZ3 : 4;
    uint32_t trbType : 6;
    uint32_t direction : 1;
    uint32_t rsvdZ4 : 15;
} __attribute__((packed)) xhci_status_trb_t;

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
    uint32_t rsvdZ[3];
    uint32_t cycleBit : 1;
    uint32_t rsvdZ2 : 9;
    uint32_t trbType : 6;
    uint32_t slotType : 5;
    uint32_t rsvdZ3 : 11;
} __attribute__((packed)) xhci_enable_slot_command_trb_t;

typedef struct {
    uint64_t inputContextPointer;
    uint32_t rsvdZ;
    uint32_t cycleBit : 1;
    uint32_t rsvdZ2 : 8;
    uint32_t blockSetAddressRequest : 1;
    uint32_t trbType : 6;
    uint32_t rsvdZ3 : 8;
    uint32_t slotID : 8;
} __attribute__((packed)) xhci_address_device_command_trb_t;

_Static_assert(XHCI_TRB_SIZE == sizeof(xhci_trb_t), "xhci_trb_t size mismatch");
_Static_assert(XHCI_TRB_SIZE == sizeof(xhci_event_trb_t), "xhci_event_trb_t size mismatch");
_Static_assert(XHCI_TRB_SIZE == sizeof(xhci_link_trb_t), "xhci_link_trb_t size mismatch");
_Static_assert(XHCI_TRB_SIZE == sizeof(xhci_transfer_event_trb_t), "xhci_transfer_event_trb_t size mismatch");

typedef struct {
    xhci_ext_cap_supported_protocol_t* protocol;
    xhci_port_regs_t* registers;
} xhci_port_t;

typedef struct {
    volatile bool completed;
    union {
        xhci_trb_t event;
    };
} xhci_command_completion_event_t;

typedef struct xhci_controller xhci_controller_t;

typedef struct {
    xhci_controller_t* hcd;
    uintptr_t physicalAddr;
    xhci_trb_t* ring;
    unsigned enqueueIndex;
    unsigned maxIndex;
    bool cycleState;
    xhci_command_completion_event_t* events;
} xhci_command_ring_t;

typedef struct {
    uintptr_t physicalAddr;
    xhci_trb_t* ring;
    unsigned enqueueIndex;
    unsigned maxIndex;
    bool cycleState;
    volatile int last_transfer_status;
} xhci_transfer_ring_t;

typedef struct {
    uintptr_t physicalAddr;
    xhci_event_trb_t* segment;
    uint32_t size;
} xhci_event_ring_segment_t;

typedef struct {
    xhci_controller_t* hcd;
    uintptr_t segmentsPhys;
    xhci_event_ring_segment_table_entry_t* segmentTable;
    xhci_event_ring_segment_t* segments;
    uint32_t segmentCount;
    uint32_t dequeueIndex;
    bool cycleState;
} xhci_event_ring_t;

typedef struct {
    uint8_t slot_id;
    uint8_t port_speed;
    xhci_device_context_t* device_context;
    uintptr_t device_context_phys;
    xhci_transfer_ring_t rings[32]; // indexed by DCI
    bool is_tablet;
} xhci_device_t;

struct xhci_controller {
    uintptr_t xhciBaseAddress;
    uintptr_t xhciVirtualAddress;

    xhci_cap_regs_t* capRegs;
    xhci_op_regs_t* opRegs;
    xhci_port_regs_t* portRegs;
    xhci_runtime_regs_t* runtimeRegs;
    xhci_doorbell_register_t* doorbellRegs;
    xhci_interrupter_t* interrupter;

    uintptr_t devContextBaseAddressArrayPhys;
    uint64_t* devContextBaseAddressArray;

    uintptr_t scratchpadBuffersPhys;
    uint64_t* scratchpadBuffers;

    uintptr_t extCapabilities;
    uint8_t maxSlots;
    uint8_t controllerIRQ;

    xhci_ext_cap_supported_protocol_t** protocols;
    uint32_t protocolCount;
    uint32_t protocolCapacity;

    xhci_port_t* ports;
    xhci_command_ring_t commandRing;
    xhci_event_ring_t eventRing;

    xhci_device_t* devices[256];

    enum {
        ControllerNotInitialized,
        ControllerInitialized,
    } controllerStatus;
};

/* USB Requests */
#define USB_REQ_GET_DESCRIPTOR  0x06
#define USB_REQ_SET_CONFIGURATION 0x09

#define USB_DESC_DEVICE         0x01
#define USB_DESC_CONFIG         0x02
#define USB_DESC_INTERFACE      0x04
#define USB_DESC_ENDPOINT       0x05

/* USB Descriptors */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed)) usb_device_descriptor_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} __attribute__((packed)) usb_config_descriptor_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
} __attribute__((packed)) usb_interface_descriptor_t;

typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} __attribute__((packed)) usb_endpoint_descriptor_t;

#endif
