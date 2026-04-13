#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "allocator.h"
#include "slab.h"
#include "pool.h"
#include "predictor.h"

/* ─────────────────────────────────────────────
   POINTER HASH MAP
   Key:   void* ptr
   Value: alloc_frame, type, size
   Collision resolution: open addressing + linear probe
   Size must be power of 2 for fast modulo via &
───────────────────────────────────────────── */
#define MAP_SIZE  16384          /* 2^14 — enough for 10k live objects  */
#define MAP_MASK  (MAP_SIZE - 1)
#define TOMBSTONE ((void*)0x1)   /* marks a deleted slot                */

typedef struct {
    void*      ptr;
    int        alloc_frame;
    ObjectType type;
    float      size;
} TrackEntry;

static TrackEntry map[MAP_SIZE];

static unsigned int hash_ptr(void* ptr)
{
    uintptr_t p = (uintptr_t)ptr;
    p ^= (p >> 16);
    p *= 0x45d9f3b;
    p ^= (p >> 16);
    return (unsigned int)(p & MAP_MASK);
}

static void map_insert(void* ptr, int frame, ObjectType type, float size)
{
    unsigned int idx = hash_ptr(ptr);
    for (int i = 0; i < MAP_SIZE; i++)
    {
        TrackEntry* e = &map[(idx + i) & MAP_MASK];
        if (!e->ptr || e->ptr == TOMBSTONE)
        {
            e->ptr         = ptr;
            e->alloc_frame = frame;
            e->type        = type;
            e->size        = size;
            return;
        }
    }
}

static TrackEntry* map_find(void* ptr)
{
    unsigned int idx = hash_ptr(ptr);
    for (int i = 0; i < MAP_SIZE; i++)
    {
        TrackEntry* e = &map[(idx + i) & MAP_MASK];
        if (!e->ptr)        return NULL;   /* empty slot — not found   */
        if (e->ptr == TOMBSTONE) continue; /* deleted slot — skip      */
        if (e->ptr == ptr)  return e;
    }
    return NULL;
}

static void map_delete(void* ptr)
{
    unsigned int idx = hash_ptr(ptr);
    for (int i = 0; i < MAP_SIZE; i++)
    {
        TrackEntry* e = &map[(idx + i) & MAP_MASK];
        if (!e->ptr)       return;
        if (e->ptr == ptr) { e->ptr = TOMBSTONE; return; }
    }
}

/* ─────────────────────────────────────────────
   ALLOCATOR STATE
───────────────────────────────────────────── */
static int slab_count   = 0;
static int pool_count   = 0;
static int malloc_count = 0;
static int current_frame = 0;

void set_current_frame(int frame) { current_frame = frame; }

void init_allocator()
{
    slab_init();
    pool_init();
    predictor_init();

    slab_count    = 0;
    pool_count    = 0;
    malloc_count  = 0;
    current_frame = 0;

    /* Clear hash map */
    for (int i = 0; i < MAP_SIZE; i++) map[i].ptr = NULL;
}

void* smart_alloc(ObjectType type, size_t size)
{
    AllocHeader* header = NULL;

    float predicted_life = predict_lifetime(type, (float)size, current_frame);
    int   use_slab       = (predicted_life < SHORT_LIVED_THRESHOLD);

    if (use_slab)
    {
        header = (AllocHeader*)slab_alloc();
        if (header)
        {
            header->source = ALLOC_SLAB;
            header->type   = type;
            header->size   = 0;
            slab_count++;
            void* ptr = (void*)(header + 1);
            map_insert(ptr, current_frame, type, (float)size);
            return ptr;
        }
    }

    header = (AllocHeader*)pool_alloc(sizeof(AllocHeader) + size);
    if (header)
    {
        header->source = ALLOC_POOL;
        header->type   = type;
        header->size   = size;
        pool_count++;
        void* ptr = (void*)(header + 1);
        map_insert(ptr, current_frame, type, (float)size);
        return ptr;
    }

    header = (AllocHeader*)malloc(sizeof(AllocHeader) + size);
    if (!header)
    {
        printf("Allocator OOM: type=%d size=%u\n", type, (unsigned int)size);
        return NULL;
    }
    header->source = ALLOC_MALLOC;
    header->type   = type;
    header->size   = size;
    malloc_count++;
    void* ptr = (void*)(header + 1);
    map_insert(ptr, current_frame, type, (float)size);
    return ptr;
}

void smart_free(void* ptr)
{
    if (!ptr) return;

    /* O(1) lookup — update predictor weights */
    TrackEntry* e = map_find(ptr);
    if (e)
    {
        update_weights(e->type, e->size, e->alloc_frame, current_frame);
        map_delete(ptr);
    }

    AllocHeader* header = (AllocHeader*)ptr - 1;
    switch (header->source)
    {
        case ALLOC_SLAB:
            slab_free(header);
            if (slab_count > 0) slab_count--;
            break;
        case ALLOC_POOL:
            pool_free(header);
            if (pool_count > 0) pool_count--;
            break;
        case ALLOC_MALLOC:
        default:
            free(header);
            if (malloc_count > 0) malloc_count--;
            break;
    }
}

void print_allocator_stats()
{
    printf("\n========================================\n");
    printf("  ALLOCATOR STATS\n");
    printf("========================================\n");
    printf("  Slab   active : %d\n", slab_count);
    printf("  Pool   active : %d\n", pool_count);
    printf("  Malloc active : %d\n", malloc_count);
    printf("========================================\n");
}