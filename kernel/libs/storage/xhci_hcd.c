#include <tusb_config.h>
#include <host/hcd.h>
#include <stdint.h>
#include <stdbool.h>

/* TinyUSB HCD Implementation for Sovereign xHCI Driver */

uint32_t tusb_time_millis_api(void) {
    extern uint64_t get_system_ticks(void);
    return (uint32_t)get_system_ticks();
}

void hcd_int_handler(uint8_t rhport, bool in_isr) {
    (void)rhport; (void)in_isr;
}

bool hcd_edpt_close(uint8_t rhport, uint8_t daddr, uint8_t ep_addr) {
    (void)rhport; (void)daddr; (void)ep_addr;
    return true;
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
    (void)rhport; (void)dev_addr; (void)ep_addr;
    return true;
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    (void)dev_addr; (void)instance; (void)report; (void)len;
}

bool hcd_init(uint8_t rhport, const tusb_rhport_init_t* rh_init) {
    (void)rhport; (void)rh_init;
    /* Our xHCI driver initializes in usb_xhci_service() during EVENT_INIT */
    return true;
}

void hcd_int_enable(uint8_t rhport) { (void)rhport; }
void hcd_int_disable(uint8_t rhport) { (void)rhport; }

uint32_t hcd_frame_number(uint8_t rhport) {
    (void)rhport;
    extern uint64_t get_system_ticks(void);
    return (uint32_t)get_system_ticks();
}

bool hcd_port_connect_status(uint8_t rhport) {
    (void)rhport;
    /* In xHCI, we monitor PORTSC CCS bit */
    extern uint32_t xhci_op_read(uint32_t reg);
    uint32_t portsc = xhci_op_read(0x400); /* Port 0 */
    return (portsc & 0x01) != 0;
}

void hcd_port_reset(uint8_t rhport) {
    (void)rhport;
    extern uint32_t xhci_op_read(uint32_t reg);
    extern void xhci_op_write(uint32_t reg, uint32_t val);
    uint32_t portsc = xhci_op_read(0x400);
    xhci_op_write(0x400, (portsc & 0xFFFF0000) | (1 << 4)); /* PR=1 */
}

void hcd_port_reset_end(uint8_t rhport) { (void)rhport; }

tusb_speed_t hcd_port_speed_get(uint8_t rhport) {
    (void)rhport;
    extern uint32_t xhci_op_read(uint32_t reg);
    uint32_t portsc = xhci_op_read(0x400);
    uint32_t speed = (portsc >> 10) & 0x0F;
    switch(speed) {
        case 1: return TUSB_SPEED_FULL;
        case 2: return TUSB_SPEED_LOW;
        case 3: return TUSB_SPEED_HIGH;
        case 4: return TUSB_SPEED_HIGH; /* xHCI speed 4 is SuperSpeed, fallback to HIGH for TUSB */
        default: return TUSB_SPEED_INVALID;
    }
}

bool hcd_edpt_open(uint8_t rhport, uint8_t daddr, tusb_desc_endpoint_t const * ep_desc) {
    (void)rhport; (void)daddr; (void)ep_desc;
    /* Map TinyUSB endpoint open to xHCI CONFIG_EP */
    return true;
}

bool hcd_setup_send(uint8_t rhport, uint8_t daddr, uint8_t const setup_packet[8]) {
    (void)rhport;
    /* Map to xhci_control_transfer */
    return true;
}

bool hcd_edpt_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr, uint8_t * buffer, uint16_t buflen) {
    (void)rhport; (void)daddr; (void)ep_addr; (void)buffer; (void)buflen;
    /* Map to xhci_transfer */
    return true;
}

bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr) {
    (void)rhport; (void)dev_addr; (void)ep_addr;
    return true;
}

void hcd_device_close(uint8_t rhport, uint8_t dev_addr) {
    (void)rhport; (void)dev_addr;
}
