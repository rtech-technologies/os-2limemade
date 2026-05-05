#include "kernel/libs/cherryusb/common/usb_osal.h"
#include "kernel/libs/cherryusb/common/usb_list.h"
#include "kernel/libs/cherryusb/common/usb_hc.h"
#include "kernel/unice64/task.h"
#include <include/stdlib.h>
#include <include/string.h>
#include <include/stdio.h>

/* OSAL implementation for OSx2 */

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size, uint32_t prio, usb_thread_entry_t entry, void *args) {
    (void)name; (void)stack_size; (void)prio; (void)args;
    register_task((void (*)(void))entry, 0);
    return (usb_osal_thread_t)get_current_task();
}

void usb_osal_thread_delete(usb_osal_thread_t thread) { (void)thread; }
void usb_osal_thread_schedule_other(void) { sys_yield(); }

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count) {
    uint32_t* sem = malloc(sizeof(uint32_t));
    if (sem) *sem = initial_count;
    return (usb_osal_sem_t)sem;
}

void usb_osal_sem_delete(usb_osal_sem_t sem) { free(sem); }
int usb_osal_sem_take(usb_osal_sem_t sem, uint32_t timeout) {
    (void)timeout;
    volatile uint32_t* s = (uint32_t*)sem;
    while (*s == 0) sys_yield();
    (*s)--;
    return 0;
}
int usb_osal_sem_give(usb_osal_sem_t sem) {
    (*(uint32_t*)sem)++;
    return 0;
}

usb_osal_mutex_t usb_osal_mutex_create(void) { return usb_osal_sem_create(1); }
void usb_osal_mutex_delete(usb_osal_mutex_t mutex) { usb_osal_sem_delete(mutex); }
int usb_osal_mutex_take(usb_osal_mutex_t mutex) { return usb_osal_sem_take(mutex, 0); }
int usb_osal_mutex_give(usb_osal_mutex_t mutex) { return usb_osal_sem_give(mutex); }

usb_osal_mq_t usb_osal_mq_create(uint32_t max_msgs) { return malloc(max_msgs * 16); }
void usb_osal_mq_delete(usb_osal_mq_t mq) { free(mq); }
int usb_osal_mq_send(usb_osal_mq_t mq, uintptr_t addr) { (void)mq; (void)addr; return 0; }
int usb_osal_mq_recv(usb_osal_mq_t mq, uintptr_t *addr, uint32_t timeout) { (void)mq; (void)addr; (void)timeout; return 0; }

void usb_osal_msleep(uint32_t delay) {
    extern void pit_wait_ms(uint32_t ms);
    pit_wait_ms(delay);
}

void* usb_osal_malloc(size_t size) { return malloc(size); }
void usb_osal_free(void* ptr) { free(ptr); }

struct usb_osal_timer *usb_osal_timer_create(const char *name, uint32_t timeout_ms, usb_timer_handler_t handler, void *argument, bool is_period) {
    (void)name; (void)timeout_ms; (void)handler; (void)argument; (void)is_period; return NULL;
}
void usb_osal_timer_start(struct usb_osal_timer *timer) { (void)timer; }
void usb_osal_timer_stop(struct usb_osal_timer *timer) { (void)timer; }
void usb_osal_timer_delete(struct usb_osal_timer *timer) { (void)timer; }

int usb_hc_init(struct usbh_bus *bus) { (void)bus; return 0; }
int usb_hc_deinit(struct usbh_bus *bus) { (void)bus; return 0; }
int usbh_roothub_control(struct usbh_bus *bus, struct usb_setup_packet *setup, uint8_t *buf) {
    (void)bus; (void)setup; (void)buf; return 0;
}
int usbh_submit_urb(struct usbh_urb *urb) { (void)urb; return 0; }
int usbh_kill_urb(struct usbh_urb *urb) { (void)urb; return 0; }

usb_osal_sem_t usb_osal_sem_create_counting(uint32_t max_count) { return usb_osal_sem_create(0); }

void USBH_IRQHandler(uint8_t busid) { (void)busid; }

/* libc stubs */

void* memset(void* s, int c, size_t n) {
    uint8_t* p = (uint8_t*)s;
    while(n--) *p++ = (uint8_t)c;
    return s;
}

void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while(n--) *d++ = *s++;
    return dest;
}

int strcmp(const char* s1, const char* s2) {
    while(*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(uint8_t*)s1 - *(uint8_t*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while(n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(uint8_t*)s1 - *(uint8_t*)s2;
}

size_t strlen(const char* s) {
    size_t len = 0;
    while(*s++) len++;
    return len;
}

char* strchr(const char* s, int c) {
    while(*s) { if (*s == (char)c) return (char*)s; s++; }
    return NULL;
}

long strtol(const char* nptr, char** endptr, int base) {
    (void)nptr; (void)endptr; (void)base; return 0;
}

int printf(const char* fmt, ...) {
    (void)fmt; return 0;
}

int snprintf(char* str, size_t size, const char* format, ...) {
    (void)str; (void)size; (void)format; return 0;
}

size_t usb_osal_enter_critical_section(void) {
    __asm__ volatile ("cli");
    return 0;
}

void usb_osal_leave_critical_section(size_t flags) {
    (void)flags;
    __asm__ volatile ("sti");
}

void __assert_fail(const char * assertion, const char * file, unsigned int line, const char * function) {
    (void)assertion; (void)file; (void)line; (void)function;
    extern void forensic_panic(const char* message, void* state);
    forensic_panic("ASSERTION FAILED", NULL);
}
