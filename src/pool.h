#ifndef POOL_H
#define POOL_H
 
#include <stddef.h>
 
void pool_init();
void* pool_alloc(size_t size);
void pool_free(void* ptr);
 
#endif