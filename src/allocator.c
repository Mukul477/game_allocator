#include <stdio.h>
#include <stdlib.h>
#include "allocator.h"
#include "slab.h"
#include "pool.h"
#include "predictor.h"

static int slab_count   = 0;
static int pool_count   = 0;
static int malloc_count = 0;

/* Track alloc frame per pointer for predictor update on free.
   Simple parallel arrays — enough for 10k live objects. */
#define MAX_TRACKED 10000

static void*      tracked_ptr[MAX_TRACKED];
static int        tracked_frame[MAX_TRACKED];
static ObjectType tracked_type[MAX_TRACKED];
static float      tracked_size[MAX_TRACKED];
static int        tracked_count = 0;

static int current_frame = 0;   /* updated by set_current_frame() */

void set_current_frame(int frame) { current_frame = frame; }

static void track_alloc(void* ptr, ObjectType type, float size)
{
    if (tracked_count >= MAX_TRACKED) return;
    tracked_ptr  [tracked_count] = ptr;
    tracked_frame[tracked_count] = current_frame;
    tracked_type [tracked_count] = type;
    tracked_size [tracked_count] = size;
    tracked_count++;
}

static void track_free(void* ptr)
{
    for (int i = 0; i < tracked_count; i++)
    {
        if (tracked_ptr[i] == ptr)
        {
            update_weights(tracked_type[i], tracked_size[i],
                           tracked_frame[i], current_frame);

            /* swap-remove */
            tracked_ptr  [i] = tracked_ptr  [tracked_count - 1];
            tracked_frame[i] = tracked_frame[tracked_count - 1];
            tracked_type [i] = tracked_type [tracked_count - 1];
            tracked_size [i] = tracked_size [tracked_count - 1];
            tracked_count--;
            return;
        }
    }
}

void init_allocator()
{
    slab_init();
    pool_init();
    predictor_init();
    slab_count   = 0;
    pool_count   = 0;
    malloc_count = 0;
    tracked_count = 0;
}

void* smart_alloc(ObjectType type, size_t size)
{
    AllocHeader* header = NULL;

    /* Ask predictor which path to take */
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
            track_alloc(ptr, type, (float)size);
            return ptr;
        }
        /* slab full — fall through to pool */
    }

    header = (AllocHeader*)pool_alloc(sizeof(AllocHeader) + size);
    if (header)
    {
        header->source = ALLOC_POOL;
        header->type   = type;
        header->size   = size;
        pool_count++;
        void* ptr = (void*)(header + 1);
        track_alloc(ptr, type, (float)size);
        return ptr;
    }

    /* pool exhausted — fall back to malloc */
    header = (AllocHeader*)malloc(sizeof(AllocHeader) + size);
    if (!header)
    {
        printf("Allocator failed: type=%d size=%u\n", type, (unsigned int)size);
        return NULL;
    }
    header->source = ALLOC_MALLOC;
    header->type   = type;
    header->size   = size;
    malloc_count++;
    void* ptr = (void*)(header + 1);
    track_alloc(ptr, type, (float)size);
    return ptr;
}

void smart_free(void* ptr)
{
    if (!ptr) return;

    track_free(ptr);   /* teach predictor before we lose the pointer */

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
    printf("  Slab  active : %d\n", slab_count);
    printf("  Pool  active : %d\n", pool_count);
    printf("  Malloc active: %d\n", malloc_count);
    printf("========================================\n");
}