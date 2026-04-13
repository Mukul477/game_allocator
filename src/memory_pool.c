#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "pool.h"

#define POOL_TOTAL (40 * 1024 * 1024)   /* 40 MB — enough for 10k bullets */

static uint8_t pool_memory[POOL_TOTAL];

typedef struct Block {
    size_t       size;   /* usable bytes after this header */
    int          free;
    struct Block* next;
} Block;

static Block* free_list = NULL;

void pool_init()
{
    free_list        = (Block*)pool_memory;
    free_list->size  = POOL_TOTAL - sizeof(Block);
    free_list->free  = 1;
    free_list->next  = NULL;
}

void* pool_alloc(size_t size)
{
    Block* curr = free_list;

    while (curr)
    {
        if (curr->free && curr->size >= size)
        {
            size_t remaining = curr->size - size;

            if (remaining > sizeof(Block) + 8)
            {
                Block* new_block =
                    (Block*)((uint8_t*)curr + sizeof(Block) + size);

                new_block->size = remaining - sizeof(Block);
                new_block->free = 1;
                new_block->next = curr->next;

                curr->next = new_block;
                curr->size = size;
            }

            curr->free = 0;
            return (void*)((uint8_t*)curr + sizeof(Block));
        }

        curr = curr->next;
    }

    return NULL;
}

void pool_free(void* ptr)
{
    if (!ptr) return;

    Block* block = (Block*)((uint8_t*)ptr - sizeof(Block));
    block->free  = 1;

    /* Coalesce forward — merge with any contiguous free neighbours */
    Block* curr = free_list;

    while (curr && curr->next)
    {
        if (curr->free && curr->next->free)
        {
            uint8_t* curr_end   = (uint8_t*)curr + sizeof(Block) + curr->size;
            uint8_t* next_start = (uint8_t*)curr->next;

            if (curr_end == next_start)
            {
                curr->size += sizeof(Block) + curr->next->size;
                curr->next  = curr->next->next;
                continue;   /* recheck same node */
            }
        }

        curr = curr->next;
    }
}