#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "slab.h"
#include "allocator.h"

/* BLOCK_SIZE must fit AllocHeader + the actual object payload.
   AllocHeader is 16 bytes on most 64-bit systems; particle payload is 32.
   128 bytes gives comfortable headroom. */
#define BLOCK_SIZE  128
#define BLOCK_COUNT 10000
#define SLAB_SIZE   (BLOCK_SIZE * BLOCK_COUNT)

static uint8_t slab_memory[SLAB_SIZE];
static void*   free_list = NULL;

typedef struct FreeNode {
    struct FreeNode* next;
} FreeNode;

void slab_init()
{
    free_list = (void*)slab_memory;

    FreeNode* current = (FreeNode*)free_list;

    for (int i = 0; i < BLOCK_COUNT - 1; i++)
    {
        FreeNode* next = (FreeNode*)((uint8_t*)current + BLOCK_SIZE);
        current->next = next;
        current = next;
    }

    current->next = NULL;
}

void* slab_alloc()
{
    if (!free_list)
        return NULL;

    FreeNode* block = (FreeNode*)free_list;
    free_list = block->next;

    return (void*)block;
}

void slab_free(void* ptr)
{
    if (!ptr) return;

    FreeNode* node = (FreeNode*)ptr;
    node->next = (FreeNode*)free_list;
    free_list = node;
}

