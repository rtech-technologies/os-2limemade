#include <kernel/libs/core/services.h>
#include <stddef.h>

#define MAX_SERVICES 16

static service_func_t services[MAX_SERVICES];
static int service_count = 0;

void register_service(service_func_t init_func) {
    if (service_count < MAX_SERVICES) {
        services[service_count++] = init_func;
    }
}

void dispatch_event(kernel_event_t event) {
    if (event == EVENT_INIT) {
        /* Standard order for INIT */
        register_service(rtc64_service);
        register_service(vga_serial_service);
        register_service(arc_mem_service);
        register_service(usb_xhci_service);
        register_service(ahci_service);
        register_service(vdisk_service);

        /* PCI Scan before VFS/Mount logic */
        void pci_scan_bus(void);
        pci_scan_bus();
    }

    for (int i = 0; i < service_count; i++) {
        if (services[i]) {
            services[i](event);
        }
    }
}
