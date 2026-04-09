#include <kernel/libs/core/services.h>
#include <kernel/unice64/task.h>
#include <include/rsl.h>
#include <stddef.h>

void vga_print(const char* fmt, ...);

#define MAX_SERVICES 16

static service_func_t services[MAX_SERVICES];
static int service_count = 0;

void register_service(service_func_t init_func) {
    if (service_count < MAX_SERVICES) {
        services[service_count++] = init_func;
    }
}

#define REQ_QUEUE_SIZE 16
static system_request_t* req_queue[REQ_QUEUE_SIZE];
static int req_head = 0;
static int req_tail = 0;
static bool worker_active = false;

static void worker_task_entry(void) {
    while (req_head != req_tail) {
        system_request_t* req = req_queue[req_head];
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

        req_head = (req_head + 1) % REQ_QUEUE_SIZE;
        sys_yield(); /* Yield between requests to let callers wake up */
    }

    worker_active = false;
    /* Work complete, transition to zombie */
    task_t* self = get_current_task();
    if (self) self->state = TASK_ZOMBIE;
    sys_yield();
}

void sovereign_request_submit(system_request_t* req) {
    task_t* current = get_current_task();
    req->caller_task = current;
    req->done = false;

    /* Enqueue Request */
    int next_tail = (req_tail + 1) % REQ_QUEUE_SIZE;
    if (next_tail == req_head) {
        vga_print("[WARN] Sovereign Request Queue Full!\n");
        /* Fallback: block caller and wait (it will eventually be picked up if it didn't enqueue, but that's bad)
           Better: Spin until space available */
        while (((req_tail + 1) % REQ_QUEUE_SIZE) == req_head) {
            sys_yield();
        }
    }

    req_queue[req_tail] = req;
    req_tail = (req_tail + 1) % REQ_QUEUE_SIZE;

    if (!worker_active) {
        worker_active = true;
        /* Spawn worker task for the queue */
        int register_transient_task(void (*entry)(void), uint32_t slab_id, uint64_t arg);
        int tid = register_transient_task(worker_task_entry, 3, 0);

        if (tid != -1) {
            /* Immediate Context Force to the worker */
            void scheduler_force_task(int task_id);
            scheduler_force_task(tid);
        } else {
            worker_active = false;
            /* Fatal: Could not spawn worker */
            vga_print("[ERROR] Failed to spawn Sovereign Worker!\n");
            return;
        }
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
