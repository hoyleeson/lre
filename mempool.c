#include <stdlib.h>

#include "mempool.h"

int mempool_init(struct mempool *pool, int size, int count)
{
    int i = 0;
    struct mem_node *node;

    pool->buf = (char *)calloc(size, count);
    pool->node = NULL;
    pool->size = size;
    pool->init_count = count;
    pool->free_count = count;

    for (i = 0 ; i < count; i++) {
        node = (struct mem_node *)(pool->buf + size * i);
        node->next = pool->node;
        pool->node = node;
    }

    return 0;
}

void *mem_alloc(struct mempool *pool)
{
    struct mem_node *node;
    if (pool->node) {
        node = pool->node;
        pool->node = node->next;
        pool->free_count--;
    } else {
        node = (struct mem_node *)malloc(pool->size);
    }

    return (void *)node;
}

void mem_free(struct mempool *pool, void *data)
{
    struct mem_node *node;

    node = (struct mem_node *)data;

    node->next = pool->node;
    pool->node = node;
    pool->free_count++;
}


void mempool_release(struct mempool *pool)
{
    struct mem_node *node, *p;

    node = pool->node;

    while (node) {
        p = node;
        node = node->next;

        if ((unsigned long)p >= (unsigned long)pool->buf &&
            (unsigned long)p < (unsigned long)(pool->buf + pool->size * pool->init_count)) {
            continue;
        }
        free(p);
    }
    free(pool->buf);
}

