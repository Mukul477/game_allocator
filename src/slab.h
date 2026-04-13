#ifndef SLAB_H
#define SLAB_H
 
#include <stddef.h>
 
void slab_init();
void* slab_alloc();
void slab_free(void* ptr);
 
#endif