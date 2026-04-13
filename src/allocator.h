#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>

typedef enum { PARTICLE, BULLET, ENEMY } ObjectType;
typedef enum { ALLOC_SLAB, ALLOC_POOL, ALLOC_MALLOC } AllocSource;

typedef struct {
    AllocSource source;
    ObjectType  type;
    size_t      size;
} AllocHeader;

void  init_allocator(void);
void  set_current_frame(int frame);   
void* smart_alloc(ObjectType type, size_t size);
void  smart_free(void* ptr);
void  print_allocator_stats(void);

#endif