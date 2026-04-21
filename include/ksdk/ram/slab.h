#ifndef KSDK_SLAB_H
#define KSDK_SLAB_H
#include <stddef.h>
void* slab_alloc_aligned(int id, size_t size, size_t align);
void* malloc(size_t size);
void free(void* ptr);
void* arc_alloc(size_t size);
#endif
