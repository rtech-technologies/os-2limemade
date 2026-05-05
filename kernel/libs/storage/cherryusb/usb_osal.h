#ifndef CHERRYUSB_OSAL_H
#define CHERRYUSB_OSAL_H

#include <stdint.h>
#include <stddef.h>

/* Minimal CherryUSB OSAL for RTECH OSx2 */
typedef void* usb_osal_thread_t;
typedef void* usb_osal_sem_t;
typedef void* usb_osal_mutex_t;

void* slab_alloc(int id, size_t size);
void slab_reset(int id);
void* malloc(size_t size);
void free(void* ptr);

static inline usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count) {
    uint32_t* sem = (uint32_t*)malloc(sizeof(uint32_t));
    if (sem) *sem = initial_count;
    return (usb_osal_sem_t)sem;
}
static inline void usb_osal_sem_delete(usb_osal_sem_t sem) { if (sem) free(sem); }
static inline int usb_osal_sem_take(usb_osal_sem_t sem, uint32_t timeout) {
    if (!sem) return -1;
    volatile uint32_t* s = (uint32_t*)sem;
    uint32_t start = 0;
    while (*s == 0) {
        if (timeout != 0xFFFFFFFF && start++ > timeout * 1000) return -1;
        __asm__ volatile ("pause");
    }
    (*s)--;
    return 0;
}
static inline int usb_osal_sem_give(usb_osal_sem_t sem) {
    if (!sem) return -1;
    (*(uint32_t*)sem)++;
    return 0;
}

void* malloc(size_t size);
void free(void* ptr);

#define usb_malloc(size) malloc(size)
#define usb_free(ptr) free(ptr)

#endif
