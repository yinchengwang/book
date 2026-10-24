/**
 * @file mm_pool.h
 * @brief 统一内存池接口
 *
 * 提供 Slab 分配器和 Arena 分配器两种内存池实现，
 * 用于减少内存碎片，提高内存分配效率。
 */
#ifndef DB_MM_POOL_H
#define DB_MM_POOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/**
 * @brief 内存池类型
 */
typedef enum {
    MM_POOL_SLAB = 0,   /**< Slab 分配器（固定块大小） */
    MM_POOL_ARENA = 1,  /**< Arena 分配器（可变块大小） */
} mm_pool_type_t;

/**
 * @brief 内存池配置
 */
typedef struct mm_pool_config_s {
    mm_pool_type_t type;        /**< 池类型 */
    size_t block_size;          /**< 块大小（Slab）或增长步长（Arena） */
    size_t max_size;            /**< 最大内存限制（0=无限制） */
    size_t initial_size;        /**< 初始预分配大小 */
    const char *name;           /**< 池名称（用于调试） */
    bool thread_safe;           /**< 是否线程安全 */
} mm_pool_config_t;

/**
 * @brief 内存池统计信息
 */
typedef struct mm_pool_stats_s {
    size_t total_allocated;     /**< 已分配总内存 */
    size_t total_used;          /**< 已使用内存 */
    size_t total_free;          /**< 空闲内存 */
    size_t num_allocations;     /**< 分配次数 */
    size_t num_frees;           /**< 释放次数 */
    size_t num_blocks;          /**< 块数量（Slab）或 chunk 数量（Arena） */
    double fragmentation;       /**< 碎片率 */
} mm_pool_stats_t;

/**
 * @brief 内存池句柄（ opaque 类型）
 */
typedef struct mm_pool_s mm_pool_t;

/* ========================================================================
 * 核心接口
 * ======================================================================== */

/**
 * @brief 创建内存池
 *
 * @param config 池配置（可为 NULL，使用默认配置）
 * @return 内存池句柄，失败返回 NULL
 *
 * @note 默认配置：
 *       - type: MM_POOL_SLAB
 *       - block_size: 256
 *       - max_size: 0 (无限制)
 *       - initial_size: 0 (不预分配)
 *       - name: "mm_pool"
 *       - thread_safe: false
 */
mm_pool_t *mm_pool_create(const mm_pool_config_t *config);

/**
 * @brief 从池中分配内存
 *
 * @param pool 内存池句柄
 * @param size 需要的字节数
 * @return 分配的内存指针，失败返回 NULL
 *
 * @note Slab 池：实际分配 ceil(size / block_size) * block_size
 *       Arena 池：实际分配 size 字节
 */
void *mm_pool_alloc(mm_pool_t *pool, size_t size);

/**
 * @brief 分配零初始化内存
 *
 * @param pool 内存池句柄
 * @param nmemb 元素数量
 * @param size 每个元素大小
 * @return 分配的内存指针，失败返回 NULL
 */
void *mm_pool_calloc(mm_pool_t *pool, size_t nmemb, size_t size);

/**
 * @brief 重新分配内存
 *
 * @param pool 内存池句柄
 * @param ptr 之前的指针（可为 NULL）
 * @param old_size 之前的大小
 * @param new_size 新的需要大小
 * @return 新分配的内存指针，失败返回 NULL
 *
 * @note 当前实现：分配新内存，复制旧数据，释放旧内存
 */
void *mm_pool_realloc(mm_pool_t *pool, void *ptr, size_t old_size, size_t new_size);

/**
 * @brief 释放内存回池
 *
 * @param pool 内存池句柄
 * @param ptr 之前分配的内存指针
 * @param size 释放的字节数
 *
 * @note Slab 池：将块放回空闲链表
 *       Arena 池：忽略（延迟释放）
 */
void mm_pool_free(mm_pool_t *pool, void *ptr, size_t size);

/**
 * @brief 重置内存池（释放所有已分配内存）
 *
 * @param pool 内存池句柄
 *
 * @warning Arena 池会立即释放所有内存
 */
void mm_pool_reset(mm_pool_t *pool);

/**
 * @brief 销毁内存池
 *
 * @param pool 内存池句柄
 *
 * @note 释放所有预分配的内存，销毁锁
 */
void mm_pool_destroy(mm_pool_t *pool);

/**
 * @brief 获取内存池统计信息
 *
 * @param pool 内存池句柄
 * @param stats 输出统计信息
 * @return 0 成功，-1 失败
 */
int mm_pool_get_stats(mm_pool_t *pool, mm_pool_stats_t *stats);

/* ========================================================================
 * 辅助接口
 * ======================================================================== */

/**
 * @brief 获取内存池类型
 * @param pool 内存池句柄
 * @return 池类型
 */
mm_pool_type_t mm_pool_get_type(const mm_pool_t *pool);

/**
 * @brief 获取块大小
 * @param pool 内存池句柄
 * @return 块大小（Slab）或增长步长（Arena）
 */
size_t mm_pool_get_block_size(const mm_pool_t *pool);

/**
 * @brief 检查内存是否属于该池
 * @param pool 内存池句柄
 * @param ptr 内存指针
 * @return true 属于该池
 */
bool mm_pool_contains(const mm_pool_t *pool, const void *ptr);

/**
 * @brief 创建 Slab 池的便捷函数
 *
 * @param block_size 块大小
 * @param initial_blocks 初始块数（0=不预分配）
 * @param name 池名称
 * @return 内存池句柄
 */
mm_pool_t *mm_pool_slab_create(size_t block_size, size_t initial_blocks, const char *name);

/**
 * @brief 创建 Arena 池的便捷函数
 *
 * @param chunk_size chunk 大小
 * @param initial_size 初始大小
 * @param name 池名称
 * @return 内存池句柄
 */
mm_pool_t *mm_pool_arena_create(size_t chunk_size, size_t initial_size, const char *name);

/* ========================================================================
 * 预定义的内存池
 * ======================================================================== */

/**
 * @brief 全局向量内存池
 * 用于向量引擎的 VecPage 管理
 */
extern mm_pool_t *g_vec_pool;

/**
 * @brief 全局图内存池
 * 用于图引擎的 CSR 存储
 */
extern mm_pool_t *g_graph_pool;

/**
 * @brief 全局时序内存池
 * 用于时序引擎的段索引
 */
extern mm_pool_t *g_ts_pool;

/**
 * @brief 全局文档内存池
 * 用于文档引擎的倒排索引
 */
extern mm_pool_t *g_doc_pool;

/**
 * @brief 初始化全局内存池
 * @param data_dir 数据目录（用于持久化配置）
 * @return 0 成功，-1 失败
 */
int mm_pool_init_global(const char *data_dir);

/**
 * @brief 销毁全局内存池
 */
void mm_pool_shutdown_global(void);

/* ========================================================================
 * 便捷宏
 * ======================================================================== */

/**
 * @brief 安全分配宏（检查 NULL）
 */
#define MM_POOL_ALLOC(pool, size) \
    (((size) == 0) ? NULL : mm_pool_alloc((pool), (size)))

/**
 * @brief 安全释放宏（检查 NULL）
 */
#define MM_POOL_FREE(pool, ptr, size) \
    do { if ((ptr) != NULL) mm_pool_free((pool), (ptr), (size)); } while (0)

#ifdef __cplusplus
}
#endif

#endif /* DB_MM_POOL_H */
