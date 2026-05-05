#ifndef CHERRYUSB_OSAL_H
#define CHERRYUSB_OSAL_H

#include <stdint.h>
#include <stddef.h>

/* Minimal CherryUSB OSAL for RTECH OSx2 */
typedef void* usb_osal_thread_t;
typedef void* usb_osal_sem_t;
typedef void* usb_osal_mutex_t;

static inline usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count) { return (void*)(uintptr_t)initial_count; }
static inline void usb_osal_sem_delete(usb_osal_sem_t sem) { (void)sem; }
static inline int usb_osal_sem_take(usb_osal_sem_t sem, uint32_t timeout) { (void)sem; (void)timeout; return 0; }
static inline int usb_osal_sem_give(usb_osal_sem_t sem) { (void)sem; return 0; }

void* malloc(size_t size);
void free(void* ptr);

#define usb_malloc(size) malloc(size)
#define usb_free(ptr) free(ptr)

#endif
