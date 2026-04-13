/* Baseline simulation using system malloc — no custom allocator */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_OBJECT 10000

typedef enum { PARTICLE, BULLET, ENEMY } ObjectType;

typedef struct {
    ObjectType type;
    size_t     size;
    int        lifetime;
    int        remaining_life;
    void*      memory;
} GameObject;

int total_alloc = 0;
int total_free  = 0;
int peak_objects = 0;

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

    GameObject game[MAX_OBJECT];
    int object_count = 0;

    for (int frame = 0; frame < 3000; frame++)
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
                free(game[j].memory);
                total_free++;
                game[j] = game[object_count - 1];
                object_count--;
                j--;
            }
        }
    }

    printf("=== Baseline (system malloc) ===\n");
    printf("Active objects:    %d\n", object_count);
    printf("Total allocations: %d\n", total_alloc);
    printf("Peak objects:      %d\n", peak_objects);

    for (int i = 0; i < object_count; i++) { free(game[i].memory); total_free++; }
    printf("Total frees:       %d\n", total_free);
    return 0;
}


