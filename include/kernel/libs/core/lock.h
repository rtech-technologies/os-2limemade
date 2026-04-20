#ifndef LOCK_H
#define LOCK_H
#include <stdint.h>
#include <stdbool.h>
void spin_lock(volatile uint32_t* lock);
void spin_unlock(volatile uint32_t* lock);
bool spin_try_lock(volatile uint32_t* lock);
#endif
