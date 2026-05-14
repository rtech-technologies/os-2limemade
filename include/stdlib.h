#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>
#include <stdbool.h>

void* malloc(size_t size);
void free(void* ptr);
bool is_slab_pointer(void* ptr);

#endif
