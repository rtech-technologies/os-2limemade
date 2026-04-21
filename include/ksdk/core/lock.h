#ifndef KSDK_LOCK_H
#define KSDK_LOCK_H
#include <stdint.h>
void spin_lock(volatile uint32_t* lock);
void spin_unlock(volatile uint32_t* lock);
#endif
