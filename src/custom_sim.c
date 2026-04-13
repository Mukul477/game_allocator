#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "allocator.h"
#include "predictor.h"

#define MAX_OBJECT 10000
#define NUM_FRAMES 3000

typedef struct {
    ObjectType type;
    size_t     size;
    int        lifetime;
    int        remaining_life;
    void*      memory;
} GameObject;

static int total_alloc   = 0;
static int total_free    = 0;
static int peak_objects  = 0;

/* Per-type counters for results table */
static int alloc_by_type[3] = {0, 0, 0};
static int slab_routed   = 0;
static int pool_routed   = 0;
static int malloc_routed = 0;

void init_object(GameObject* G, int frame)
{
    if      (G->type == PARTICLE) { G->size = 32;   G->lifetime = 3;   }
    else if (G->type == BULLET)   { G->size = 128;  G->lifetime = 50;  }
    else                          { G->size = 1024; G->lifetime = 500; }

    G->remaining_life = G->lifetime;
    G->memory = smart_alloc(G->type, G->size);

    if (!G->memory)
    {
        printf("smart_alloc failed: type=%d size=%u\n",
               G->type, (unsigned int)G->size);
        exit(1);
    }

    total_alloc++;
    alloc_by_type[G->type]++;

    /* Check which path was taken via header */
    AllocHeader* h = (AllocHeader*)G->memory - 1;
    if      (h->source == ALLOC_SLAB)   slab_routed++;
    else if (h->source == ALLOC_POOL)   pool_routed++;
    else                                malloc_routed++;

    (void)frame;
}

int main()
{
    srand(42);

    init_allocator();

    GameObject game[MAX_OBJECT];
    int object_count = 0;

    for (int frame = 0; frame < NUM_FRAMES; frame++)
    {
        set_current_frame(frame);

        if (object_count < MAX_OBJECT)
        {
            for (int k = 0; k < 10; k++)
            {
                if (object_count >= MAX_OBJECT) break;

                int r = rand() % 100;
                if      (r < 70) game[object_count].type = PARTICLE;
                else if (r < 95) game[object_count].type = BULLET;
                else             game[object_count].type = ENEMY;

                init_object(&game[object_count], frame);
                object_count++;
            }
        }

        if (object_count > peak_objects) peak_objects = object_count;

        for (int j = 0; j < object_count; j++)
        {
            game[j].remaining_life--;
            if (game[j].remaining_life <= 0)
            {
                smart_free(game[j].memory);
                total_free++;
                game[j] = game[object_count - 1];
                object_count--;
                j--;
            }
        }
    }

    /* ── RESULTS ────────────────────────────── */
    printf("\n========================================\n");
    printf("   CUSTOM ALLOCATOR SIMULATION\n");
    printf("========================================\n");

    printf("\n--- Simulation summary ---\n");
    printf("  Frames simulated  : %d\n",  NUM_FRAMES);
    printf("  Total allocations : %d\n",  total_alloc);
    printf("  Peak live objects : %d\n",  peak_objects);
    printf("  Active at end     : %d\n",  object_count);

    printf("\n--- Allocations by object type ---\n");
    printf("  Particle : %d  (lifetime=3  → slab target)\n",  alloc_by_type[PARTICLE]);
    printf("  Bullet   : %d  (lifetime=50 → pool target)\n",  alloc_by_type[BULLET]);
    printf("  Enemy    : %d  (lifetime=500→ malloc target)\n", alloc_by_type[ENEMY]);

    printf("\n--- Allocator routing (predictor decisions) ---\n");
    printf("  Routed to slab   : %d  (%.1f%%)\n",
           slab_routed,   (float)slab_routed   / total_alloc * 100.0f);
    printf("  Routed to pool   : %d  (%.1f%%)\n",
           pool_routed,   (float)pool_routed   / total_alloc * 100.0f);
    printf("  Routed to malloc : %d  (%.1f%%)\n",
           malloc_routed, (float)malloc_routed / total_alloc * 100.0f);

    print_allocator_stats();
    predictor_print_stats();

    /* Clean up remaining live objects */
    for (int i = 0; i < object_count; i++)
    {
        smart_free(game[i].memory);
        total_free++;
    }
    printf("\n  Total frees       : %d\n", total_free);
    printf("  Alloc==Free check : %s\n\n",
           total_alloc == total_free ? "PASS" : "FAIL");

    return 0;
}