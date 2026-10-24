/**
 * @file vector_buf.h
 * @brief 向量索引与 Buffer Pool 集成接口
 *
 * ============================================================================
 * 设计目标
 * ============================================================================
 *
 * 将向量索引页面纳入 Buffer Pool 管理，实现：
 * - 热点向量数据的缓存
 * - 脏页刷盘协调
 * - 页面加载优化
 * - 缓存预热
 *
 * ============================================================================
 * 架构设计
 * ============================================================================
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                      向量引擎                                    │
 * │   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐            │
 * │   │  HNSW Index │  │ DiskANN     │  │  IVF-PQ     │            │
 * │   └──────┬──────┘  └──────┬──────┘  └──────┬──────┘            │
 * │          │                │                │                    │
 * │          └────────────────┼────────────────┘                    │
 * │                           ▼                                     │
 * │                    ┌─────────────────┐                         │
 * │                    │ VectorBuf API   │                         │
 * │                    │ (缓存抽象层)    │                         │
 * │                    └────────┬────────┘                         │
 * └─────────────────────────────┼───────────────────────────────────┘
 *                               │
 *                    ┌──────────▼──────────┐
 *                    │    Buffer Pool      │
 *                    │  (db/buf.h)         │
 *                    └─────────────────────┘
 * ```
 *
 * ============================================================================
 * 页面类型
 * ============================================================================
 *
 * | 类型 ID | 名称          | 说明                    |
 * |---------|---------------|------------------------|
 * | 0xB0    | VEC_PAGE      | 向量数据页              |
 * | 0xB1    | VEC_INDEX     | 向量索引页              |
 * | 0xB2    | VEC_META      | 向量元数据页            |
 * | 0xB3    | VEC_WAL_META  | WAL 元数据页            |
 *
 */
#ifndef VECTOR_BUF_H
#define VECTOR_BUF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

/** 向量 Buffer 池 (前向声明) */
typedef struct vector_buf_pool_s vector_buf_pool_t;

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 向量页面类型 */
#define VECTOR_PAGE_TYPE_DATA   0xB0  /**< 向量数据页 */
#define VECTOR_PAGE_TYPE_INDEX  0xB1  /**< 向量索引页 */
#define VECTOR_PAGE_TYPE_META   0xB2  /**< 向量元数据页 */
#define VECTOR_PAGE_TYPE_WAL    0xB3  /**< WAL 元数据页 */

/** 向量 Buffer 池默认大小 */
#define VECTOR_BUF_DEFAULT_SIZE 256

/** 向量页面默认值 */
#define VECTOR_BUF_PAGE_SIZE 8192

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/**
 * @brief 向量 Buffer 描述符
 */
typedef struct vector_buf_desc_s {
    int32_t  buf_id;           /**< Buffer ID */
    uint32_t segment_id;       /**< 段 ID */
    uint32_t page_no;          /**< 页面号 */
    uint8_t  page_type;        /**< 页面类型 */
    bool     is_dirty;         /**< 是否脏页 */
    bool     is_pinned;        /**< 是否被 pin */
    uint32_t ref_count;        /**< 引用计数 */
} vector_buf_desc_t;

/**
 * @brief 向量 Buffer 句柄
 */
typedef struct vector_buf_s {
    vector_buf_desc_t desc;    /**< 描述符 */
    void             *data;    /**< 页面数据 */
} vector_buf_t;

/** 向量 Buffer 池 (前向声明) */
typedef struct vector_buf_pool_s vector_buf_pool_t;

/**
 * @brief 向量 Buffer 池配置
 */
typedef struct vector_buf_config_s {
    uint32_t pool_size;        /**< 池大小 */
    uint32_t page_size;        /**< 页面大小 */
    bool     prefetch_enabled; /**< 是否启用预取 */
    uint32_t prefetch_distance; /**< 预取距离 */
} vector_buf_config_t;

/**
 * @brief 缓存统计信息
 */
typedef struct vector_buf_stats_s {
    uint64_t hits;             /**< 缓存命中数 */
    uint64_t misses;           /**< 缓存未命中数 */
    uint64_t writes;           /**< 写入数 */
    uint64_t evictions;        /**< 淘汰数 */
    uint32_t current_size;     /**< 当前大小 */
    uint32_t max_size;         /**< 最大大小 */
} vector_buf_stats_t;

/* ========================================================================
 * 缓存 API
 * ======================================================================== */

/**
 * @brief 创建向量 Buffer 池
 * @param config 配置 (可为 NULL 使用默认配置)
 * @return Buffer 池指针，失败返回 NULL
 */
void *vector_buf_create(const vector_buf_config_t *config);

/**
 * @brief 销毁向量 Buffer 池
 * @param pool Buffer 池指针
 */
void vector_buf_destroy(void *pool);

/**
 * @brief 获取向量页面
 * @param pool Buffer 池指针
 * @param segment_id 段 ID
 * @param page_no 页面号
 * @param page_type 页面类型
 * @return Buffer 句柄，失败返回 NULL
 */
vector_buf_t *vector_buf_get(void *pool, uint32_t segment_id,
                             uint32_t page_no, uint8_t page_type);

/**
 * @brief 释放向量页面
 * @param buf Buffer 句柄
 */
void vector_buf_release(vector_buf_t *buf);

/**
 * @brief 标记页面为脏
 * @param buf Buffer 句柄
 */
void vector_buf_mark_dirty(vector_buf_t *buf);

/**
 * @brief 刷新脏页到磁盘
 * @param pool Buffer 池指针
 * @param segment_id 段 ID (0 表示所有段)
 * @return 刷新的页面数
 */
int vector_buf_flush(void *pool, uint32_t segment_id);

/**
 * @brief 预取向量页面
 * @param pool Buffer 池指针
 * @param segment_id 段 ID
 * @param start_page 起始页面号
 * @param count 页面数
 */
void vector_buf_prefetch(void *pool, uint32_t segment_id,
                         uint32_t start_page, uint32_t count);

/* ========================================================================
 * 统计和监控
 * ======================================================================== */

/**
 * @brief 获取缓存统计
 * @param pool Buffer 池指针
 * @param stats 输出统计 (不为 NULL)
 */
void vector_buf_get_stats(void *pool, vector_buf_stats_t *stats);

/**
 * @brief 重置统计
 * @param pool Buffer 池指针
 */
void vector_buf_reset_stats(void *pool);

/**
 * @brief 获取缓存命中率
 * @param pool Buffer 池指针
 * @return 命中率 (0.0 - 1.0)
 */
double vector_buf_hit_rate(void *pool);

/* ========================================================================
 * 缓存预热
 * ======================================================================== */

/**
 * @brief 从文件预热缓存
 * @param pool Buffer 池指针
 * @param segment_id 段 ID
 * @param start_page 起始页面号
 * @param count 页面数
 * @return 预热的页面数
 */
int vector_buf_warmup(void *pool, uint32_t segment_id,
                      uint32_t start_page, uint32_t count);

/**
 * @brief 获取预热进度
 * @param pool Buffer 池指针
 * @param segment_id 段 ID
 * @return 预热进度 (0.0 - 1.0)
 */
double vector_buf_warmup_progress(void *pool, uint32_t segment_id);

/* ========================================================================
 * 脏页管理
 * ======================================================================== */

/**
 * @brief 获取脏页数量
 * @param pool Buffer 池指针
 * @param segment_id 段 ID (0 表示所有段)
 * @return 脏页数量
 */
uint32_t vector_buf_dirty_count(void *pool, uint32_t segment_id);

/**
 * @brief 检查页面是否为脏
 * @param buf Buffer 句柄
 * @return 是否为脏
 */
bool vector_buf_is_dirty(const vector_buf_t *buf);

/* ========================================================================
 * 内存管理
 * ======================================================================== */

/**
 * @brief 获取缓存内存使用
 * @param pool Buffer 池指针
 * @return 内存使用 (bytes)
 */
uint64_t vector_buf_memory_usage(void *pool);

/**
 * @brief 设置缓存大小限制
 * @param pool Buffer 池指针
 * @param max_size 最大大小
 * @return 0 成功
 */
int vector_buf_set_max_size(void *pool, uint32_t max_size);

/**
 * @brief 驱逐最少使用的页面
 * @param pool Buffer 池指针
 * @return 驱逐的页面数
 */
uint32_t vector_buf_evict_lru(void *pool);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 创建默认配置
 * @return 默认配置
 */
vector_buf_config_t vector_buf_default_config(void);

/**
 * @brief 打印缓存状态
 * @param pool Buffer 池指针
 */
void vector_buf_print_status(void *pool);

#ifdef __cplusplus
}
#endif

#endif /* VECTOR_BUF_H */
