#ifndef KSDK_TIMER_H
#define KSDK_TIMER_H
#include <stdint.h>
void pit_wait_ms(uint32_t ms);
uint64_t get_system_ticks(void);
#endif
