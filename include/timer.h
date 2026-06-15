#ifndef OS_TIMER_H
#define OS_TIMER_H

#include <stdint.h>

uint64_t get_system_ticks(void);
void pit_wait_ms(uint32_t ms);

#endif
