#ifndef KBASE_INFRA_MEMORY_H
#define KBASE_INFRA_MEMORY_H

#include <stddef.h>
#include "infra/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 内存池（基于 mm_pool 封装） */
typedef struct infra_memory_pool {
    void* mm_pool;       /* 指向 mm_pool_t */
    size_t initial_size;
    size_t max_size;
    size_t used_bytes;
} infra_memory_pool_t;

/* 创建内存池 */
infra_memory_pool_t* infra_memory_pool_create(size_t initial_size, size_t max_size);

/* 销毁内存池 */
void infra_memory_pool_destroy(infra_memory_pool_t* pool);

/* 分配 */
void* infra_memory_pool_alloc(infra_memory_pool_t* pool, size_t size);

/* calloc */
void* infra_memory_pool_calloc(infra_memory_pool_t* pool, size_t nmemb, size_t size);

/* 重置（释放所有分配） */
void infra_memory_pool_reset(infra_memory_pool_t* pool);

#ifdef __cplusplus
}
#endif

#endif /* KBASE_INFRA_MEMORY_H */