#ifndef SERVICES_H
#define SERVICES_H

#include <stdint.h>

typedef enum {
    EVENT_INIT,
    EVENT_MAIN,
    EVENT_CLEANUP,
    EVENT_EXIT
} kernel_event_t;

typedef void (*service_func_t)(kernel_event_t event);

void register_service(service_func_t init_func);
void dispatch_event(kernel_event_t event);

/* Core Hardware Drivers */
void vga_serial_service(kernel_event_t event);
void usb_xhci_service(kernel_event_t event);
void ahci_service(kernel_event_t event);
void ahci_hardware_audit(int p);
void arc_mem_service(kernel_event_t event);
void vdisk_service(kernel_event_t event);
void rtc64_service(kernel_event_t event);

#endif /* SERVICES_H */
