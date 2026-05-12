#include <include/tusb_config.h>
#include <tusb.h>
#include <stdint.h>

/* TinyUSB Board Hooks */
uint32_t board_millis(void) {
    extern uint64_t get_system_ticks(void);
    return (uint32_t)get_system_ticks();
}

/* TinyUSB Host Event Hooks */
void tuh_mount_cb(uint8_t dev_addr) {
    (void)dev_addr;
    void serial_write_str(const char* s);
    serial_write_str("[TUSB] Device Mounted.\n");
}

void tuh_umount_cb(uint8_t dev_addr) {
    (void)dev_addr;
    void serial_write_str(const char* s);
    serial_write_str("[TUSB] Device Unmounted.\n");
}

/* TinyUSB Sovereign Task Entry */
void tiny_usb_task(void) {
    void vga_print(const char* fmt, ...);
    vga_print("[TUSB] TinyUSB Integration Task Active.\n");

    tuh_init(BOARD_TUH_RHPORT);

    while (1) {
        tuh_task();
        void sys_yield(void);
        sys_yield();
    }
}
