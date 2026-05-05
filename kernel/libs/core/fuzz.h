#ifndef FUZZ_H
#define FUZZ_H

#include <stdint.h>

void syscall_fuzz_harness(void);
void driver_ioctl_fuzz(void);

#endif
