#include "infra/memory.h"
#include <stdlib.h>
#include <string.h>
#include <db/mm_pool.h>

infra_memory_pool_t* infra_memory_pool_create(size_t initial_size, size_t max_size) {
    mm_pool_config_t cfg = {
        .type = MM_POOL_ARENA,
        .block_size = 4096,
        .max_size = max_size,
        .initial_size = initial_size,
        .name = "infra_pool",
        .thread_safe = false,
    };
    mm_pool_t* mm = mm_pool_create(&cfg);
    if (!mm) return NULL;

    infra_memory_pool_t* pool = (infra_memory_pool_t*)calloc(1, sizeof(*pool));
    if (!pool) {
        mm_pool_destroy(mm);
        return NULL;
    }
    pool->mm_pool = mm;
    pool->initial_size = initial_size;
    pool->max_size = max_size;
    return pool;
}

void infra_memory_pool_destroy(infra_memory_pool_t* pool) {
    if (!pool) return;
    if (pool->mm_pool) mm_pool_destroy((mm_pool_t*)pool->mm_pool);
    free(pool);
}

void* infra_memory_pool_alloc(infra_memory_pool_t* pool, size_t size) {
    if (!pool || !pool->mm_pool || size == 0) return NULL;
    void* p = mm_pool_alloc((mm_pool_t*)pool->mm_pool, size);
    if (p) pool->used_bytes += size;
    return p;
}

void* infra_memory_pool_calloc(infra_memory_pool_t* pool, size_t nmemb, size_t size) {
    if (!pool || !pool->mm_pool || nmemb == 0 || size == 0) return NULL;
    void* p = mm_pool_calloc((mm_pool_t*)pool->mm_pool, nmemb, size);
    if (p) pool->used_bytes += nmemb * size;
    return p;
}

void infra_memory_pool_reset(infra_memory_pool_t* pool) {
    if (!pool || !pool->mm_pool) return;
    mm_pool_reset((mm_pool_t*)pool->mm_pool);
    pool->used_bytes = 0;
}