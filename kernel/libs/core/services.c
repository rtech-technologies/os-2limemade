#include <kernel/libs/core/services.h>
#include <kernel/unice64/task.h>
#include <include/rsl.h>
#include <stddef.h>

#define MAX_SERVICES 16

static service_func_t services[MAX_SERVICES];
static int service_count = 0;

void register_service(service_func_t init_func) {
    if (service_count < MAX_SERVICES) {
        services[service_count++] = init_func;
    }
}

static system_request_t* current_req = NULL;

static void worker_task_entry(void) {
    if (!current_req) {
        task_t* self = get_current_task();
        if (self) self->state = TASK_ZOMBIE;
        sys_yield();
        return;
    }

    system_request_t* req = current_req;
    req->result = 0;

    switch (req->type) {
        case REQ_FS_LS:
            rsl_ls(req->path);
            break;
        case REQ_FS_CAT:
            rsl_cat(req->path);
            break;
        case REQ_FS_WRITE:
            rsl_write(req->path, req->content);
            break;
        case REQ_FS_MKDIR:
            rsl_mkdir(req->path);
            break;
        case REQ_DISK_MOUNT:
            rsl_mount(req->path);
            break;
        case REQ_DISK_FORMAT:
            rsl_format(req->path);
            break;
        case REQ_DISK_STAMP:
            rsl_stamp(req->path);
            break;
        case REQ_DISK_EJECT:
            rsl_eject(req->path);
            break;
        case REQ_HARDWARE_SCAN:
            rsl_scan();
            break;
    }

    req->done = true;
    if (req->caller_task) {
        ((task_t*)req->caller_task)->state = TASK_READY;
    }
    current_req = NULL;

    /* Work complete, transition to zombie */
    task_t* self = get_current_task();
    if (self) self->state = TASK_ZOMBIE;
    sys_yield();
}

void sovereign_request_submit(system_request_t* req) {
    task_t* current = get_current_task();
    req->caller_task = current;
    req->done = false;
    current_req = req;

    /* Spawn worker task for this request */
    int register_transient_task(void (*entry)(void), uint32_t slab_id, uint64_t arg);
    int tid = register_transient_task(worker_task_entry, 3, 0);

    if (tid != -1) {
        /* Immediate Context Force to the worker */
        void scheduler_force_task(int task_id);
        scheduler_force_task(tid);
    } else {
        /* Degraded: Run synchronously in current context if spawn fails */
        worker_task_entry();
        return;
    }

    /* Transition caller to wait state and yield */
    if (current) current->state = TASK_WAITING;
    sys_yield();
}

void sovereign_service_orchestrator(void) {
    /* Maintenance task now only performs background audits */
    void ahci_hardware_audit(int p);
    for (int i=0; i<8; i++) ahci_hardware_audit(i);
    sys_yield();
}

void dispatch_event(kernel_event_t event) {
    if (event == EVENT_INIT) {
        /* Standard order for INIT */
        register_service(vga_serial_service);
        register_service(arc_mem_service);
        register_service(usb_xhci_service);
        void nvme_service(kernel_event_t event);
        register_service(nvme_service);
        register_service(ahci_service);
        register_service(vdisk_service);
    }

    for (int i = 0; i < service_count; i++) {
        if (services[i]) {
            services[i](event);
        }
    }
}
