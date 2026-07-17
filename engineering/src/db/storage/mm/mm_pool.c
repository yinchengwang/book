/**
 * @file mm_pool.c
 * @brief 统一内存池实现
 *
 * 实现 Slab 分配器和 Arena 分配器。
 */
#include "db/mm_pool.h"
#include "db/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION pthread_mutex_t;
#define pthread_mutex_init(m, a) InitializeCriticalSection(m)
#define pthread_mutex_destroy(m) DeleteCriticalSection(m)
#define pthread_mutex_lock(m) EnterCriticalSection(m)
#define pthread_mutex_unlock(m) LeaveCriticalSection(m)
#else
#include <pthread.h>
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

#define MM_POOL_MAGIC 0x4D4D504F4F4C2020UL  /* "MM POOL  " */
#define DEFAULT_BLOCK_SIZE 256
#define MIN_BLOCK_SIZE 8
#define MAX_BLOCK_SIZE (1024 * 1024 * 64)  /* 64MB */
#define MIN_CHUNK_SIZE 4096
#define DEFAULT_CHUNK_SIZE (1024 * 1024)    /* 1MB */

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/**
 * @brief Slab 块信息
 */
typedef struct mm_slab_block_s {
    void *memory;                    /**< 内存块起始地址 */
    size_t block_size;               /**< 块大小 */
    uint32_t free_count;             /**< 空闲块数量 */
    uint32_t total_count;            /**< 总块数量 */
    uint32_t *free_list;             /**< 空闲块索引数组 */
    uint32_t free_list_capacity;     /**< 空闲列表容量 */
    struct mm_slab_block_s *next;    /**< 下一个 slab */
} mm_slab_block_t;

/**
 * @brief Slab 分配器
 */
typedef struct mm_slab_s {
    uint64_t magic;                  /**< 魔数 */
    size_t block_size;               /**< 块大小 */
    size_t max_size;                 /**< 最大内存限制 */
    size_t total_allocated;          /**< 已分配总内存 */
    size_t total_used;               /**< 已使用内存 */
    size_t num_allocations;          /**< 分配次数 */
    size_t num_frees;                /**< 释放次数 */
    size_t num_blocks;               /**< 块数量 */
    mm_slab_block_t *blocks;         /**< slab 块链表 */
    pthread_mutex_t mutex;           /**< 并发锁 */
    bool thread_safe;                /**< 是否线程安全 */
    char name[64];                   /**< 池名称 */
} mm_slab_t;

/**
 * @brief Arena chunk
 */
typedef struct mm_arena_chunk_s {
    void *memory;                    /**< chunk 内存 */
    size_t size;                     /**< chunk 大小 */
    size_t used;                     /**< 已使用大小 */
    struct mm_arena_chunk_s *next;   /**< 下一个 chunk */
} mm_arena_chunk_t;

/**
 * @brief Arena 分配器
 */
typedef struct mm_arena_s {
    uint64_t magic;                  /**< 魔数 */
    size_t chunk_size;               /**< chunk 大小 */
    size_t max_size;                 /**< 最大内存限制 */
    size_t total_allocated;          /**< 已分配总内存 */
    size_t total_used;               /**< 已使用内存 */
    size_t num_allocations;          /**< 分配次数 */
    size_t num_frees;                /**< 释放次数 */
    size_t num_chunks;               /**< chunk 数量 */
    mm_arena_chunk_t *chunks;        /**< chunk 链表 */
    mm_arena_chunk_t *current;       /**< 当前 chunk */
    pthread_mutex_t mutex;           /**< 并发锁 */
    bool thread_safe;                /**< 是否线程安全 */
    char name[64];                   /**< 池名称 */
} mm_arena_t;

/**
 * @brief 内存池通用头
 */
struct mm_pool_s {
    uint64_t magic;                  /**< 魔数 */
    mm_pool_type_t type;             /**< 池类型 */
    union {
        mm_slab_t slab;              /**< Slab 分配器 */
        mm_arena_t arena;            /**< Arena 分配器 */
    } u;
    char name[64];                   /**< 池名称 */
};

/* ========================================================================
 * Slab 分配器实现
 * ======================================================================== */

/**
 * @brief 创建新的 slab 块
 */
static mm_slab_block_t *slab_block_create(size_t block_size, size_t num_blocks) {
    if (block_size == 0 || num_blocks == 0) return NULL;

    mm_slab_block_t *block = (mm_slab_block_t *)calloc(1, sizeof(mm_slab_block_t));
    if (block == NULL) return NULL;

    size_t total_size = block_size * num_blocks;
    block->memory = malloc(total_size);
    if (block->memory == NULL) {
        free(block);
        return NULL;
    }

    block->block_size = block_size;
    block->total_count = (uint32_t)num_blocks;
    block->free_count = block->total_count;
    block->next = NULL;

    /* 初始化空闲列表 */
    block->free_list_capacity = block->total_count;
    block->free_list = (uint32_t *)malloc(sizeof(uint32_t) * block->total_count);
    if (block->free_list == NULL) {
        free(block->memory);
        free(block);
        return NULL;
    }

    /* 初始时所有块都空闲 */
    for (uint32_t i = 0; i < block->total_count; i++) {
        block->free_list[i] = i;
    }

    return block;
}

/**
 * @brief 销毁 slab 块
 */
static void slab_block_destroy(mm_slab_block_t *block) {
    if (block == NULL) return;
    if (block->memory) free(block->memory);
    if (block->free_list) free(block->free_list);
    free(block);
}

/**
 * @brief 从 slab 块分配
 */
static void *slab_block_alloc(mm_slab_block_t *block) {
    if (block == NULL || block->free_count == 0) return NULL;

    uint32_t idx = block->free_list[block->free_count - 1];
    block->free_count--;

    return (char *)block->memory + idx * block->block_size;
}

/**
 * @brief 释放到 slab 块
 */
static bool slab_block_free(mm_slab_block_t *block, void *ptr) {
    if (block == NULL || ptr == NULL) return false;

    char *mem = (char *)block->memory;
    char *p = (char *)ptr;

    /* 检查指针是否属于这个 block */
    if (p < mem || p >= mem + block->block_size * block->total_count) {
        return false;
    }

    uint32_t idx = (uint32_t)((p - mem) / block->block_size);

    /* 检查是否已经释放 */
    for (uint32_t i = block->free_count; i < block->total_count; i++) {
        if (block->free_list[i] == idx) {
            return false;  /* 已经释放过了 */
        }
    }

    /* 放回空闲列表 */
    block->free_list[block->free_count] = idx;
    block->free_count++;

    return true;
}

/**
 * @brief 创建 Slab 分配器
 */
static mm_pool_t *mm_slab_create(const mm_pool_config_t *config) {
    mm_pool_config_t cfg;
    if (config == NULL) {
        memset(&cfg, 0, sizeof(cfg));
        cfg.type = MM_POOL_SLAB;
        cfg.block_size = DEFAULT_BLOCK_SIZE;
        cfg.name = "mm_pool";
        cfg.thread_safe = false;
        config = &cfg;
    }

    size_t block_size = config->block_size;
    if (block_size < MIN_BLOCK_SIZE) block_size = MIN_BLOCK_SIZE;
    if (block_size > MAX_BLOCK_SIZE) block_size = MAX_BLOCK_SIZE;

    /* 对齐到 8 字节 */
    block_size = (block_size + 7) & ~7UL;

    mm_pool_t *pool = (mm_pool_t *)calloc(1, sizeof(mm_pool_t));
    if (pool == NULL) return NULL;

    pool->magic = MM_POOL_MAGIC;
    pool->type = MM_POOL_SLAB;

    mm_slab_t *slab = &pool->u.slab;
    slab->magic = MM_POOL_MAGIC;
    slab->block_size = block_size;
    slab->max_size = config->max_size;
    slab->total_allocated = 0;
    slab->total_used = 0;
    slab->num_allocations = 0;
    slab->num_frees = 0;
    slab->num_blocks = 0;
    slab->blocks = NULL;
    slab->thread_safe = config->thread_safe;
    strncpy(slab->name, config->name ? config->name : "slab_pool", sizeof(slab->name) - 1);

    if (slab->thread_safe) {
        pthread_mutex_init(&slab->mutex, NULL);
    }

    /* 如果指定了 initial_size，预分配初始块 */
    if (config->initial_size > 0) {
        size_t num_blocks = config->initial_size / block_size;
        if (num_blocks == 0) num_blocks = 1;
        mm_slab_block_t *block = slab_block_create(block_size, num_blocks);
        if (block != NULL) {
            block->next = slab->blocks;
            slab->blocks = block;
            slab->num_blocks++;
            slab->total_allocated += block_size * num_blocks;
        }
    }

    return pool;
}

/**
 * @brief Slab 分配
 */
static void *mm_slab_alloc(mm_pool_t *pool, size_t size) {
    if (pool == NULL || pool->magic != MM_POOL_MAGIC || pool->type != MM_POOL_SLAB) {
        return NULL;
    }
    if (size == 0) return NULL;

    mm_slab_t *slab = &pool->u.slab;
    void *ptr = NULL;

    if (slab->thread_safe) {
        pthread_mutex_lock(&slab->mutex);
    }

    /* 检查是否超出最大限制 */
    if (slab->max_size > 0 && slab->total_used + size > slab->max_size) {
        if (slab->thread_safe) pthread_mutex_unlock(&slab->mutex);
        errno = ENOMEM;
        return NULL;
    }

    /* 尝试从现有块分配 */
    mm_slab_block_t *block = slab->blocks;
    while (block != NULL) {
        if (block->free_count > 0) {
            ptr = slab_block_alloc(block);
            if (ptr != NULL) {
                slab->total_used += slab->block_size;
                slab->num_allocations++;
                break;
            }
        }
        block = block->next;
    }

    /* 如果没有可用块，创建新块 */
    if (ptr == NULL) {
        /* 计算新块的大小：至少 16 个块，或足够满足当前请求 */
        size_t num_blocks = 16;
        size_t needed_blocks = (size + slab->block_size - 1) / slab->block_size;
        if (needed_blocks > num_blocks) num_blocks = needed_blocks;

        /* 检查是否超出最大限制 */
        size_t new_size = slab->block_size * num_blocks;
        if (slab->max_size > 0 && slab->total_allocated + new_size > slab->max_size) {
            if (slab->thread_safe) pthread_mutex_unlock(&slab->mutex);
            errno = ENOMEM;
            return NULL;
        }

        block = slab_block_create(slab->block_size, num_blocks);
        if (block != NULL) {
            block->next = slab->blocks;
            slab->blocks = block;
            slab->num_blocks++;
            slab->total_allocated += new_size;

            ptr = slab_block_alloc(block);
            if (ptr != NULL) {
                slab->total_used += slab->block_size;
                slab->num_allocations++;
            }
        }
    }

    if (slab->thread_safe) {
        pthread_mutex_unlock(&slab->mutex);
    }

    return ptr;
}

/**
 * @brief Slab 释放
 */
static void mm_slab_free(mm_pool_t *pool, void *ptr, size_t size) {
    (void)size;  /* Slab 不使用 size 参数 */

    if (pool == NULL || pool->magic != MM_POOL_MAGIC || pool->type != MM_POOL_SLAB) {
        return;
    }
    if (ptr == NULL) return;

    mm_slab_t *slab = &pool->u.slab;

    if (slab->thread_safe) {
        pthread_mutex_lock(&slab->mutex);
    }

    /* 在所有块中查找 */
    mm_slab_block_t *block = slab->blocks;
    while (block != NULL) {
        if (slab_block_free(block, ptr)) {
            slab->total_used -= slab->block_size;
            slab->num_frees++;
            break;
        }
        block = block->next;
    }

    if (slab->thread_safe) {
        pthread_mutex_unlock(&slab->mutex);
    }
}

/**
 * @brief Slab 重置
 */
static void mm_slab_reset(mm_pool_t *pool) {
    if (pool == NULL || pool->magic != MM_POOL_MAGIC || pool->type != MM_POOL_SLAB) {
        return;
    }

    mm_slab_t *slab = &pool->u.slab;

    if (slab->thread_safe) {
        pthread_mutex_lock(&slab->mutex);
    }

    /* 释放所有块（保留第一个） */
    mm_slab_block_t *block = slab->blocks;
    if (block != NULL) {
        slab->blocks = block->next;
        slab_block_destroy(block);
        slab->num_blocks = 0;

        /* 重新初始化第一个块 */
        block = slab_block_create(slab->block_size, 16);
        if (block != NULL) {
            block->next = slab->blocks;
            slab->blocks = block;
            slab->num_blocks = 1;
            slab->total_allocated = slab->block_size * 16;
        }
        slab->total_used = 0;
    }

    if (slab->thread_safe) {
        pthread_mutex_unlock(&slab->mutex);
    }
}

/**
 * @brief Slab 销毁
 */
static void mm_slab_destroy(mm_pool_t *pool) {
    if (pool == NULL || pool->magic != MM_POOL_MAGIC || pool->type != MM_POOL_SLAB) {
        return;
    }

    mm_slab_t *slab = &pool->u.slab;

    if (slab->thread_safe) {
        pthread_mutex_lock(&slab->mutex);
    }

    /* 释放所有块 */
    mm_slab_block_t *block = slab->blocks;
    while (block != NULL) {
        mm_slab_block_t *next = block->next;
        slab_block_destroy(block);
        block = next;
    }
    slab->blocks = NULL;

    if (slab->thread_safe) {
        pthread_mutex_unlock(&slab->mutex);
        pthread_mutex_destroy(&slab->mutex);
    }

    pool->magic = 0;
    free(pool);
}

/**
 * @brief Slab 统计
 */
static int mm_slab_get_stats(mm_pool_t *pool, mm_pool_stats_t *stats) {
    if (pool == NULL || pool->magic != MM_POOL_MAGIC || pool->type != MM_POOL_SLAB) {
        return -1;
    }
    if (stats == NULL) return -1;

    mm_slab_t *slab = &pool->u.slab;

    memset(stats, 0, sizeof(mm_pool_stats_t));

    if (slab->thread_safe) {
        pthread_mutex_lock(&slab->mutex);
    }

    stats->total_allocated = slab->total_allocated;
    stats->total_used = slab->total_used;
    stats->total_free = slab->total_allocated - slab->total_used;
    stats->num_allocations = slab->num_allocations;
    stats->num_frees = slab->num_frees;
    stats->num_blocks = slab->num_blocks;

    /* 计算碎片率：空闲块比例 */
    size_t free_blocks = 0;
    mm_slab_block_t *block = slab->blocks;
    while (block != NULL) {
        free_blocks += block->free_count;
        block = block->next;
    }
    size_t total_blocks = 0;
    block = slab->blocks;
    while (block != NULL) {
        total_blocks += block->total_count;
        block = block->next;
    }
    stats->fragmentation = total_blocks > 0 ?
        (double)free_blocks / total_blocks : 0.0;

    if (slab->thread_safe) {
        pthread_mutex_unlock(&slab->mutex);
    }

    return 0;
}

/* ========================================================================
 * Arena 分配器实现
 * ======================================================================== */

/**
 * @brief 创建新的 chunk
 */
static mm_arena_chunk_t *arena_chunk_create(size_t size) {
    if (size == 0) return NULL;

    mm_arena_chunk_t *chunk = (mm_arena_chunk_t *)calloc(1, sizeof(mm_arena_chunk_t));
    if (chunk == NULL) return NULL;

    chunk->memory = malloc(size);
    if (chunk->memory == NULL) {
        free(chunk);
        return NULL;
    }

    chunk->size = size;
    chunk->used = 0;
    chunk->next = NULL;

    return chunk;
}

/**
 * @brief 销毁 chunk
 */
static void arena_chunk_destroy(mm_arena_chunk_t *chunk) {
    if (chunk == NULL) return;
    if (chunk->memory) free(chunk->memory);
    free(chunk);
}

/**
 * @brief 创建 Arena 分配器
 */
static mm_pool_t *mm_arena_create(const mm_pool_config_t *config) {
    mm_pool_config_t cfg;
    if (config == NULL) {
        memset(&cfg, 0, sizeof(cfg));
        cfg.type = MM_POOL_ARENA;
        cfg.block_size = DEFAULT_CHUNK_SIZE;
        cfg.name = "mm_arena";
        cfg.thread_safe = false;
        config = &cfg;
    }

    size_t chunk_size = config->block_size;
    if (chunk_size < MIN_CHUNK_SIZE) chunk_size = MIN_CHUNK_SIZE;
    chunk_size = (chunk_size + 4095) & ~4095UL;  /* 对齐到 4KB */

    mm_pool_t *pool = (mm_pool_t *)calloc(1, sizeof(mm_pool_t));
    if (pool == NULL) return NULL;

    pool->magic = MM_POOL_MAGIC;
    pool->type = MM_POOL_ARENA;

    mm_arena_t *arena = &pool->u.arena;
    arena->magic = MM_POOL_MAGIC;
    arena->chunk_size = chunk_size;
    arena->max_size = config->max_size;
    arena->total_allocated = 0;
    arena->total_used = 0;
    arena->num_allocations = 0;
    arena->num_frees = 0;
    arena->num_chunks = 0;
    arena->chunks = NULL;
    arena->current = NULL;
    arena->thread_safe = config->thread_safe;
    strncpy(arena->name, config->name ? config->name : "arena_pool", sizeof(arena->name) - 1);

    if (arena->thread_safe) {
        pthread_mutex_init(&arena->mutex, NULL);
    }

    /* 如果指定了 initial_size，预分配初始 chunk */
    if (config->initial_size > 0) {
        size_t init_size = config->initial_size;
        if (init_size < chunk_size) init_size = chunk_size;
        mm_arena_chunk_t *chunk = arena_chunk_create(init_size);
        if (chunk != NULL) {
            chunk->next = arena->chunks;
            arena->chunks = chunk;
            arena->current = chunk;
            arena->num_chunks++;
            arena->total_allocated += init_size;
        }
    }

    return pool;
}

/**
 * @brief Arena 分配
 */
static void *mm_arena_alloc(mm_pool_t *pool, size_t size) {
    if (pool == NULL || pool->magic != MM_POOL_MAGIC || pool->type != MM_POOL_ARENA) {
        return NULL;
    }
    if (size == 0) return NULL;

    mm_arena_t *arena = &pool->u.arena;

    /* 对齐到 8 字节 */
    size = (size + 7) & ~7UL;

    if (arena->thread_safe) {
        pthread_mutex_lock(&arena->mutex);
    }

    /* 检查是否超出最大限制 */
    if (arena->max_size > 0 && arena->total_used + size > arena->max_size) {
        if (arena->thread_safe) pthread_mutex_unlock(&arena->mutex);
        errno = ENOMEM;
        return NULL;
    }

    void *ptr = NULL;

    /* 尝试从当前 chunk 分配 */
    if (arena->current != NULL) {
        if (arena->current->used + size <= arena->current->size) {
            ptr = (char *)arena->current->memory + arena->current->used;
            arena->current->used += size;
            arena->total_used += size;
            arena->num_allocations++;
        }
    }

    /* 如果当前 chunk 不够，创建新 chunk */
    if (ptr == NULL) {
        size_t new_size = arena->chunk_size;
        if (size > new_size) {
            new_size = size;  /* 大分配使用精确大小 */
        }

        /* 检查是否超出最大限制 */
        if (arena->max_size > 0 && arena->total_allocated + new_size > arena->max_size) {
            if (arena->thread_safe) pthread_mutex_unlock(&arena->mutex);
            errno = ENOMEM;
            return NULL;
        }

        mm_arena_chunk_t *chunk = arena_chunk_create(new_size);
        if (chunk != NULL) {
            chunk->next = arena->chunks;
            arena->chunks = chunk;
            arena->current = chunk;
            arena->num_chunks++;
            arena->total_allocated += new_size;

            ptr = (char *)chunk->memory + chunk->used;
            chunk->used = size;
            arena->total_used += size;
            arena->num_allocations++;
        }
    }

    if (arena->thread_safe) {
        pthread_mutex_unlock(&arena->mutex);
    }

    return ptr;
}

/**
 * @brief Arena 释放（延迟释放）
 */
static void mm_arena_free(mm_pool_t *pool, void *ptr, size_t size) {
    (void)ptr;
    (void)size;  /* Arena 延迟释放，忽略 free 调用 */

    if (pool == NULL || pool->magic != MM_POOL_MAGIC || pool->type != MM_POOL_ARENA) {
        return;
    }

    mm_arena_t *arena = &pool->u.arena;

    if (arena->thread_safe) {
        pthread_mutex_lock(&arena->mutex);
    }

    arena->num_frees++;
    /* Arena 不实际释放内存，只记录释放次数 */

    if (arena->thread_safe) {
        pthread_mutex_unlock(&arena->mutex);
    }
}

/**
 * @brief Arena 重置
 */
static void mm_arena_reset(mm_pool_t *pool) {
    if (pool == NULL || pool->magic != MM_POOL_MAGIC || pool->type != MM_POOL_ARENA) {
        return;
    }

    mm_arena_t *arena = &pool->u.arena;

    if (arena->thread_safe) {
        pthread_mutex_lock(&arena->mutex);
    }

    /* 释放所有 chunk */
    mm_arena_chunk_t *chunk = arena->chunks;
    while (chunk != NULL) {
        mm_arena_chunk_t *next = chunk->next;
        arena_chunk_destroy(chunk);
        chunk = next;
    }
    arena->chunks = NULL;
    arena->current = NULL;
    arena->num_chunks = 0;
    arena->total_allocated = 0;
    arena->total_used = 0;

    /* 创建新的初始 chunk */
    mm_arena_chunk_t *new_chunk = arena_chunk_create(arena->chunk_size);
    if (new_chunk != NULL) {
        new_chunk->next = arena->chunks;
        arena->chunks = new_chunk;
        arena->current = new_chunk;
        arena->num_chunks = 1;
        arena->total_allocated = arena->chunk_size;
    }

    if (arena->thread_safe) {
        pthread_mutex_unlock(&arena->mutex);
    }
}

/**
 * @brief Arena 销毁
 */
static void mm_arena_destroy(mm_pool_t *pool) {
    if (pool == NULL || pool->magic != MM_POOL_MAGIC || pool->type != MM_POOL_ARENA) {
        return;
    }

    mm_arena_t *arena = &pool->u.arena;

    if (arena->thread_safe) {
        pthread_mutex_lock(&arena->mutex);
    }

    /* 释放所有 chunk */
    mm_arena_chunk_t *chunk = arena->chunks;
    while (chunk != NULL) {
        mm_arena_chunk_t *next = chunk->next;
        arena_chunk_destroy(chunk);
        chunk = next;
    }
    arena->chunks = NULL;
    arena->current = NULL;

    if (arena->thread_safe) {
        pthread_mutex_unlock(&arena->mutex);
        pthread_mutex_destroy(&arena->mutex);
    }

    pool->magic = 0;
    free(pool);
}

/**
 * @brief Arena 统计
 */
static int mm_arena_get_stats(mm_pool_t *pool, mm_pool_stats_t *stats) {
    if (pool == NULL || pool->magic != MM_POOL_MAGIC || pool->type != MM_POOL_ARENA) {
        return -1;
    }
    if (stats == NULL) return -1;

    mm_arena_t *arena = &pool->u.arena;

    memset(stats, 0, sizeof(mm_pool_stats_t));

    if (arena->thread_safe) {
        pthread_mutex_lock(&arena->mutex);
    }

    stats->total_allocated = arena->total_allocated;
    stats->total_used = arena->total_used;
    stats->total_free = arena->total_allocated - arena->total_used;
    stats->num_allocations = arena->num_allocations;
    stats->num_frees = arena->num_frees;
    stats->num_blocks = arena->num_chunks;
    stats->fragmentation = 0.0;  /* Arena 没有碎片 */

    if (arena->thread_safe) {
        pthread_mutex_unlock(&arena->mutex);
    }

    return 0;
}

/* ========================================================================
 * 统一接口实现
 * ======================================================================== */

mm_pool_t *mm_pool_create(const mm_pool_config_t *config) {
    mm_pool_config_t cfg;
    if (config == NULL) {
        memset(&cfg, 0, sizeof(cfg));
        cfg.type = MM_POOL_SLAB;
        cfg.block_size = DEFAULT_BLOCK_SIZE;
        cfg.name = "mm_pool";
        cfg.thread_safe = false;
        config = &cfg;
    }

    if (config->type == MM_POOL_ARENA) {
        return mm_arena_create(config);
    } else {
        return mm_slab_create(config);
    }
}

void *mm_pool_alloc(mm_pool_t *pool, size_t size) {
    if (pool == NULL || pool->magic != MM_POOL_MAGIC) {
        return NULL;
    }

    if (pool->type == MM_POOL_SLAB) {
        return mm_slab_alloc(pool, size);
    } else {
        return mm_arena_alloc(pool, size);
    }
}

void *mm_pool_calloc(mm_pool_t *pool, size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = mm_pool_alloc(pool, total);
    if (ptr != NULL) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *mm_pool_realloc(mm_pool_t *pool, void *ptr, size_t old_size, size_t new_size) {
    if (new_size == 0) {
        mm_pool_free(pool, ptr, old_size);
        return NULL;
    }

    if (ptr == NULL) {
        return mm_pool_alloc(pool, new_size);
    }

    void *new_ptr = mm_pool_alloc(pool, new_size);
    if (new_ptr == NULL) {
        return NULL;
    }

    /* 复制旧数据 */
    size_t copy_size = old_size < new_size ? old_size : new_size;
    memcpy(new_ptr, ptr, copy_size);

    /* 释放旧内存 */
    mm_pool_free(pool, ptr, old_size);

    return new_ptr;
}

void mm_pool_free(mm_pool_t *pool, void *ptr, size_t size) {
    if (pool == NULL || pool->magic != MM_POOL_MAGIC) {
        return;
    }

    if (pool->type == MM_POOL_SLAB) {
        mm_slab_free(pool, ptr, size);
    } else {
        mm_arena_free(pool, ptr, size);
    }
}

void mm_pool_reset(mm_pool_t *pool) {
    if (pool == NULL || pool->magic != MM_POOL_MAGIC) {
        return;
    }

    if (pool->type == MM_POOL_SLAB) {
        mm_slab_reset(pool);
    } else {
        mm_arena_reset(pool);
    }
}

void mm_pool_destroy(mm_pool_t *pool) {
    if (pool == NULL || pool->magic != MM_POOL_MAGIC) {
        return;
    }

    if (pool->type == MM_POOL_SLAB) {
        mm_slab_destroy(pool);
    } else {
        mm_arena_destroy(pool);
    }
}

int mm_pool_get_stats(mm_pool_t *pool, mm_pool_stats_t *stats) {
    if (pool == NULL || pool->magic != MM_POOL_MAGIC) {
        return -1;
    }

    if (pool->type == MM_POOL_SLAB) {
        return mm_slab_get_stats(pool, stats);
    } else {
        return mm_arena_get_stats(pool, stats);
    }
}

mm_pool_type_t mm_pool_get_type(const mm_pool_t *pool) {
    if (pool == NULL || pool->magic != MM_POOL_MAGIC) {
        return MM_POOL_SLAB;
    }
    return pool->type;
}

size_t mm_pool_get_block_size(const mm_pool_t *pool) {
    if (pool == NULL || pool->magic != MM_POOL_MAGIC) {
        return 0;
    }

    if (pool->type == MM_POOL_SLAB) {
        return pool->u.slab.block_size;
    } else {
        return pool->u.arena.chunk_size;
    }
}

bool mm_pool_contains(const mm_pool_t *pool, const void *ptr) {
    if (pool == NULL || pool->magic != MM_POOL_MAGIC || ptr == NULL) {
        return false;
    }

    if (pool->type == MM_POOL_SLAB) {
        const mm_slab_t *slab = &pool->u.slab;
        const mm_slab_block_t *block = slab->blocks;
        while (block != NULL) {
            const char *mem = (const char *)block->memory;
            if (ptr >= mem && ptr < mem + block->block_size * block->total_count) {
                return true;
            }
            block = block->next;
        }
    } else {
        const mm_arena_t *arena = &pool->u.arena;
        const mm_arena_chunk_t *chunk = arena->chunks;
        while (chunk != NULL) {
            if (ptr >= chunk->memory && ptr < (char *)chunk->memory + chunk->size) {
                return true;
            }
            chunk = chunk->next;
        }
    }

    return false;
}

mm_pool_t *mm_pool_slab_create(size_t block_size, size_t initial_blocks, const char *name) {
    mm_pool_config_t config = {
        .type = MM_POOL_SLAB,
        .block_size = block_size,
        .max_size = 0,
        .initial_size = block_size * initial_blocks,
        .name = name ? name : "slab_pool",
        .thread_safe = true
    };
    return mm_pool_create(&config);
}

mm_pool_t *mm_pool_arena_create(size_t chunk_size, size_t initial_size, const char *name) {
    mm_pool_config_t config = {
        .type = MM_POOL_ARENA,
        .block_size = chunk_size,
        .max_size = 0,
        .initial_size = initial_size,
        .name = name ? name : "arena_pool",
        .thread_safe = true
    };
    return mm_pool_create(&config);
}

/* ========================================================================
 * 全局内存池
 * ======================================================================== */

mm_pool_t *g_vec_pool = NULL;
mm_pool_t *g_graph_pool = NULL;
mm_pool_t *g_ts_pool = NULL;
mm_pool_t *g_doc_pool = NULL;

int mm_pool_init_global(const char *data_dir) {
    (void)data_dir;

    /* 创建全局向量池 - Slab 池，512 字节块（适合向量数据） */
    g_vec_pool = mm_pool_slab_create(512, 1024, "vec_pool");
    if (g_vec_pool == NULL) {
        LOG_ERROR("创建全局向量池失败");
        return -1;
    }

    /* 创建全局图池 - Slab 池，64 字节块（适合边数据） */
    g_graph_pool = mm_pool_slab_create(64, 4096, "graph_pool");
    if (g_graph_pool == NULL) {
        LOG_ERROR("创建全局图池失败");
        mm_pool_destroy(g_vec_pool);
        g_vec_pool = NULL;
        return -1;
    }

    /* 创建全局时序池 - Arena 池，1MB chunk（适合时序段） */
    g_ts_pool = mm_pool_arena_create(1024 * 1024, 4 * 1024 * 1024, "ts_pool");
    if (g_ts_pool == NULL) {
        LOG_ERROR("创建全局时序池失败");
        mm_pool_destroy(g_vec_pool);
        mm_pool_destroy(g_graph_pool);
        g_vec_pool = NULL;
        g_graph_pool = NULL;
        return -1;
    }

    /* 创建全局文档池 - Arena 池，256KB chunk（适合倒排列表） */
    g_doc_pool = mm_pool_arena_create(256 * 1024, 1024 * 1024, "doc_pool");
    if (g_doc_pool == NULL) {
        LOG_ERROR("创建全局文档池失败");
        mm_pool_destroy(g_vec_pool);
        mm_pool_destroy(g_graph_pool);
        mm_pool_destroy(g_ts_pool);
        g_vec_pool = NULL;
        g_graph_pool = NULL;
        g_ts_pool = NULL;
        return -1;
    }

    LOG_INFO("全局内存池初始化成功");
    return 0;
}

void mm_pool_shutdown_global(void) {
    if (g_vec_pool) {
        mm_pool_destroy(g_vec_pool);
        g_vec_pool = NULL;
    }
    if (g_graph_pool) {
        mm_pool_destroy(g_graph_pool);
        g_graph_pool = NULL;
    }
    if (g_ts_pool) {
        mm_pool_destroy(g_ts_pool);
        g_ts_pool = NULL;
    }
    if (g_doc_pool) {
        mm_pool_destroy(g_doc_pool);
        g_doc_pool = NULL;
    }
    LOG_INFO("全局内存池已关闭");
}
