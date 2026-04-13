#ifndef POOL_H
#define POOL_H

#include <stddef.h>

void  pool_init();
void* pool_alloc(size_t size);
void  pool_free(void* ptr);

/* Results metrics */
int   pool_count_fragments();
float pool_fragmentation_percent();

#endif