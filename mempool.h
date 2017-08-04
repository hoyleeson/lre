#ifndef _LRE_MEMPOOL_H_
#define _LRE_MEMPOOL_H_

struct mem_node {
    struct mem_node *next;
};

struct mempool {
    char *buf;
    struct mem_node *node;

    int size;
    int init_count;
    int free_count;
};

int mempool_init(struct mempool *pool, int size, int count);
void *mem_alloc(struct mempool *pool);
void mem_free(struct mempool *pool, void *data);
void mempool_release(struct mempool *pool);

#endif

