#include <stdint.h>
#include <stddef.h>

void* usb_osal_malloc(size_t size) {
    extern void* arc_alloc(size_t size);
    return arc_alloc(size);
}

void usb_osal_free(void* ptr) {
    extern void release(void* ptr);
    release(ptr);
}

uint64_t usb_osal_get_ticks(void) {
    extern uint64_t get_system_ticks(void);
    return get_system_ticks();
}

void usb_osal_msleep(uint32_t ms) {
    extern void pit_wait_ms(uint32_t ms);
    pit_wait_ms(ms);
}
