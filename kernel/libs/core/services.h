#ifndef SERVICES_H
#define SERVICES_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    EVENT_INIT,
    EVENT_MAIN,
    EVENT_CLEANUP,
    EVENT_EXIT
} kernel_event_t;

typedef void (*service_func_t)(kernel_event_t event);

void register_service(service_func_t init_func);
void dispatch_event(kernel_event_t event);

/* Sovereign Service Orchestrator */
typedef enum {
    REQ_FS_LS,
    REQ_FS_CAT,
    REQ_FS_WRITE,
    REQ_FS_MKDIR,
    REQ_DISK_MOUNT,
    REQ_DISK_FORMAT,
    REQ_DISK_STAMP,
    REQ_DISK_EJECT,
    REQ_HARDWARE_SCAN,
    REQ_APP_SPAWN
} system_req_type_t;

typedef struct {
    system_req_type_t type;
    void* path;
    void* content;
    int drive_id;
    volatile bool done;
    volatile int result;
    void* caller_task;
} system_request_t;

void sovereign_request_submit(system_request_t* req);
void sovereign_service_orchestrator(void);

/* Core Hardware Drivers */
void vga_serial_service(kernel_event_t event);
void usb_xhci_service(kernel_event_t event);
void ahci_service(kernel_event_t event);
void ahci_hardware_audit(int p);
void arc_mem_service(kernel_event_t event);
void vdisk_service(kernel_event_t event);
void mouse_service(kernel_event_t event);

#endif /* SERVICES_H */
