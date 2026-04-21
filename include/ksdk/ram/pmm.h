#ifndef KSDK_PMM_H
#define KSDK_PMM_H
#include <stdint.h>
void* pmm_alloc(uint64_t count);
uint64_t vmm_get_phys(void* virt);
uint64_t get_hhdm_offset(void);
#endif
