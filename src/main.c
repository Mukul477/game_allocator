#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_OBJECT 10000
#define NUM_FRAMES 3000

typedef enum { PARTICLE, BULLET, ENEMY } ObjectType;

typedef struct {
    ObjectType type;
    size_t     size;
    int        lifetime;
    int        remaining_life;
    void*      memory;
} GameObject;

static int total_alloc  = 0;
static int total_free   = 0;
static int peak_objects = 0;

/* Fragmentation proxy:
   Track how many distinct free-size events happen —
   each differently-sized free punches a different hole */
static int frag_events  = 0;
static size_t last_free_size = 0;

void init_object(GameObject* G)
{
    if      (G->type == PARTICLE) { G->size = 32;   G->lifetime = 3;   }
    else if (G->type == BULLET)   { G->size = 128;  G->lifetime = 50;  }
    else                          { G->size = 1024; G->lifetime = 500; }

    G->remaining_life = G->lifetime;
    G->memory = malloc(G->size);
    if (!G->memory) { printf("malloc failed\n"); exit(1); }
    total_alloc++;
}

int main()
{
    srand(42);

    clock_t start_time = clock();

    GameObject game[MAX_OBJECT];
    int object_count = 0;

    for (int frame = 0; frame < NUM_FRAMES; frame++)
    {
        if (object_count < MAX_OBJECT)
        {
            for (int k = 0; k < 10; k++)
            {
                if (object_count >= MAX_OBJECT) break;
                int r = rand() % 100;
                if      (r < 70) game[object_count].type = PARTICLE;
                else if (r < 95) game[object_count].type = BULLET;
                else             game[object_count].type = ENEMY;
                init_object(&game[object_count]);
                object_count++;
            }
        }

        if (object_count > peak_objects) peak_objects = object_count;

        for (int j = 0; j < object_count; j++)
        {
            game[j].remaining_life--;
            if (game[j].remaining_life <= 0)
            {
                if (game[j].size != last_free_size) frag_events++;
                last_free_size = game[j].size;
                free(game[j].memory);
                total_free++;
                game[j] = game[object_count - 1];
                object_count--;
                j--;
            }
        }
    }

    clock_t end_time = clock();
    double time_us = ((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000000.0;

    printf("\n========================================\n");
    printf("   BASELINE SIMULATION (system malloc)\n");
    printf("========================================\n");
    printf("\n--- Simulation summary ---\n");
    printf("  Frames simulated  : %d\n", NUM_FRAMES);
    printf("  Total allocations : %d\n", total_alloc);
    printf("  Peak live objects : %d\n", peak_objects);
    printf("  Active at end     : %d\n", object_count);
    printf("  Total time        : %.2f us\n", time_us);

    printf("\n--- Fragmentation (estimated) ---\n");
    printf("  Mixed-size free events : %d\n", frag_events);
    printf("  (each = potential heap hole of different size)\n");
    printf("  System heap control    : NONE\n");
    printf("  Pool/slab isolation    : NONE\n");

    for (int i = 0; i < object_count; i++) { free(game[i].memory); total_free++; }
    printf("\n  Total frees       : %d\n", total_free);
    printf("  Alloc==Free check : %s\n\n",
           total_alloc == total_free ? "PASS" : "FAIL");
    return 0;
}


