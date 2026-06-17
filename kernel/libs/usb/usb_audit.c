#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <include/vfs.h>
#include <include/rsl.h>

typedef enum {
    SNAP_INFO,
    SNAP_DEBUG,
    SNAP_AUDIT
} snap_level_t;

static snap_level_t g_snap_level = SNAP_INFO;

void usb_audit_set_level(snap_level_t level) {
    g_snap_level = level;
}

void usb_audit_log(const char* event, const char* details) {
    extern void serial_write_str(const char* s);
    extern size_t strlen(const char* s);

    /* Human summary to serial */
    serial_write_str("[USB-SNAP] ");
    serial_write_str(event);
    serial_write_str(": ");
    serial_write_str(details);
    serial_write_str("\n");

    /* Machine-parsable JSONL to /var/log/usb-snap.log if BOOT is mounted */
    void* log_dir = str_create("BOOT:/var/log");
    if (vfs_exists(log_dir)) {
        void* log_path = str_create("BOOT:/var/log/usb-snap.log");
        vfs_handle_internal_t* h = vfs_open(log_path, "a");
        if (h) {
            const char* head = "{\"event\":\"";
            const char* mid = "\",\"details\":\"";
            const char* tail = "\"}\n";
            vfs_write(h, (void*)head, strlen(head));
            vfs_write(h, (void*)event, strlen(event));
            vfs_write(h, (void*)mid, strlen(mid));
            vfs_write(h, (void*)details, strlen(details));
            vfs_write(h, (void*)tail, strlen(tail));
            vfs_close(h);
            release(h);
        }
        release(log_path);
    }
    release(log_dir);
}
