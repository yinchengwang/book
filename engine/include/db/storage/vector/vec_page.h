/**
 * @file vector_page.h
 * @brief 向量页管理模块
 *
 * 提供向量数据的分页存储和内存管理，支持：
 * - 内存页分配和 LRU 置换
 * - 磁盘持久化
 * - Clock-Sweep 置换策略
 */
#ifndef VECTOR_PAGE_H
#define VECTOR_PAGE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 默认页大小 (4KB) */
#define VECTOR_PAGE_SIZE_DEFAULT 4096

/** 默认最大页数 */
#define VECTOR_PAGE_MAX_PAGES_DEFAULT 1024

/** 每页最大向量数上限 */
#define VECTOR_PAGE_MAX_VECTORS_PER_PAGE 1024

/** 页文件魔数 */
#define VECTOR_PAGE_FILE_MAGIC 0x56504753  /* "VPGS" */

/** 页文件版本 */
#define VECTOR_PAGE_FILE_VERSION 1

/** mmap 是否启用 */
#define VECTOR_PAGE_USE_MMAP 0

/** checksum 类型 */
#define VECTOR_PAGE_CHECKSUM_NONE 0
#define VECTOR_PAGE_CHECKSUM_CRC32 1
#define VECTOR_PAGE_CHECKSUM_ADLER32 2

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/**
 * @brief 向量页头
 */
typedef struct vector_page_header_s {
    int32_t page_id;           /**< 页 ID */
    int32_t vector_count;      /**< 页内向量数 */
    int32_t capacity;          /**< 页容量（向量数） */
    int32_t next_page;         /**< 下一页 ID，-1 表示无下一页 */
    bool dirty;                /**< 是否脏页 */
    uint64_t lsn;              /**< 日志序列号 */
    uint32_t magic;            /**< 魔数校验 */
    uint32_t checksum;         /**< 数据校验和 */
    int32_t checksum_type;     /**< 校验和类型 */
} vector_page_header_t;

/**
 * @brief 向量页
 */
typedef struct vector_page_s {
    vector_page_header_t header;   /**< 页头 */
    float *vectors;                /**< 向量数据区 [capacity * dim] */
    int32_t *vector_ids;           /**< 向量 ID 数组 [capacity] */
    void *meta;                    /**< 关联元数据（可选） */
} vector_page_t;

/**
 * @brief 页池
 */
typedef struct vector_page_pool_s {
    char data_dir[512];            /**< 数据目录 */
    int32_t max_pages;             /**< 最大页面数 */
    int32_t page_size;              /**< 页大小 */
    int32_t vector_dim;            /**< 向量维度 */
    int32_t vectors_per_page;       /**< 每页向量数 */

    /* 页数组 */
    vector_page_t **pages;          /**< 页指针数组 */
    int32_t page_count;             /**< 内存中页数 */

    /* 置换相关 */
    int32_t *clock_bits;           /**< Clock 位数组 [page_count] */
    int32_t clock_hand;             /**< Clock 指针 */

    /* 统计 */
    uint64_t hits;                  /**< 缓存命中 */
    uint64_t misses;                /**< 缓存未命中 */
    uint64_t evictions;             /**< 驱逐次数 */
    uint64_t flushes;               /**< 刷盘次数 */

    /* 持久化文件句柄 */
    void *file;                     /**< 文件句柄（实现时转为 FILE*） */

    /* mmap 支持 */
    void *mmap_base;                /**< mmap 起始地址 */
    size_t mmap_size;               /**< mmap 大小 */
    bool use_mmap;                  /**< 是否启用 mmap */

    /* 校验和配置 */
    int checksum_type;              /**< 校验和类型 */
} vector_page_pool_t;

/**
 * @brief 页统计信息
 */
typedef struct vector_page_stats_s {
    int32_t page_count;             /**< 内存中页数 */
    int32_t max_pages;              /**< 最大页数 */
    int32_t vectors_total;           /**< 总向量数 */
    uint64_t hits;                  /**< 命中数 */
    uint64_t misses;                /**< 未命中数 */
    uint64_t evictions;             /**< 驱逐次数 */
    uint64_t flushes;               /**< 刷盘次数 */
    double hit_rate;                /**< 命中率 */
} vector_page_stats_t;

/* ========================================================================
 * 生命周期 API
 * ======================================================================== */

/**
 * @brief 创建向量页池
 *
 * @param data_dir 数据目录
 * @param max_pages 最大页数
 * @param page_size 页大小
 * @param vector_dim 向量维度
 * @return 页池指针，失败返回 NULL
 */
vector_page_pool_t *vector_page_pool_create(const char *data_dir,
                                            int32_t max_pages,
                                            int32_t page_size,
                                            int32_t vector_dim);

/**
 * @brief 销毁向量页池
 *
 * @param pool 页池
 */
void vector_page_pool_destroy(vector_page_pool_t *pool);

/**
 * @brief 从文件加载页池状态
 *
 * @param pool 页池
 * @return 0 成功，-1 失败
 */
int vector_page_pool_load(vector_page_pool_t *pool);

/**
 * @brief 保存页池状态到文件
 *
 * @param pool 页池
 * @return 0 成功，-1 失败
 */
int vector_page_pool_save(const vector_page_pool_t *pool);

/* ========================================================================
 * 页操作 API
 * ======================================================================== */

/**
 * @brief 分配新向量到页
 *
 * @param pool 页池
 * @param vector 向量数据
 * @param vector_id 向量 ID（可为 -1，由页池分配）
 * @return 向量 ID，-1 表示失败
 */
int32_t vector_page_append(vector_page_pool_t *pool,
                           const float *vector,
                           int32_t vector_id);

/**
 * @brief 批量追加向量
 *
 * @param pool 页池
 * @param vectors 向量数组
 * @param count 向量数量
 * @param start_id 起始 ID（可为 -1）
 * @return 成功追加的数量
 */
int32_t vector_page_append_batch(vector_page_pool_t *pool,
                                 const float *vectors,
                                 int32_t count,
                                 int32_t start_id);

/**
 * @brief 获取向量所在的页
 *
 * @param pool 页池
 * @param vector_id 向量 ID
 * @return 向量页指针（内部引用，外部勿释放）
 */
vector_page_t *vector_page_get(vector_page_pool_t *pool, int32_t vector_id);

/**
 * @brief 获取向量数据
 *
 * @param pool 页池
 * @param vector_id 向量 ID
 * @param out_vector 输出缓冲区
 * @return 0 成功，-1 失败
 */
int vector_page_get_vector(const vector_page_pool_t *pool,
                           int32_t vector_id,
                           float *out_vector);

/**
 * @brief 驱逐最少使用页（Clock-Sweep）
 *
 * @param pool 页池
 * @return 被驱逐的页 ID，-1 表示失败
 */
int32_t vector_page_evict(vector_page_pool_t *pool);

/**
 * @brief 刷新脏页到磁盘
 *
 * @param pool 页池
 * @param page_id 页 ID，-1 表示所有脏页
 * @return 0 成功，-1 失败
 */
int vector_page_flush(vector_page_pool_t *pool, int32_t page_id);

/**
 * @brief 刷新所有脏页
 *
 * @param pool 页池
 * @return 0 成功，-1 失败
 */
int vector_page_flush_all(vector_page_pool_t *pool);

/* ========================================================================
 * 统计 API
 * ======================================================================== */

/**
 * @brief 获取页池统计信息
 *
 * @param pool 页池
 * @param stats 输出统计
 */
void vector_page_get_stats(const vector_page_pool_t *pool,
                           vector_page_stats_t *stats);

/**
 * @brief 重置统计信息
 *
 * @param pool 页池
 */
void vector_page_reset_stats(vector_page_pool_t *pool);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 计算每页可存储的向量数
 *
 * @param page_size 页大小
 * @param vector_dim 向量维度
 * @return 每页向量数
 */
int32_t vector_page_capacity(int32_t page_size, int32_t vector_dim);

/**
 * @brief 获取向量所在的页 ID
 *
 * @param vector_id 向量 ID
 * @param vectors_per_page 每页向量数
 * @return 页 ID
 */
int32_t vector_page_id(int32_t vector_id, int32_t vectors_per_page);

/**
 * @brief 获取向量在页内的偏移
 *
 * @param vector_id 向量 ID
 * @param vectors_per_page 每页向量数
 * @return 页内偏移
 */
int32_t vector_page_offset(int32_t vector_id, int32_t vectors_per_page);

/* ========================================================================
 * mmap 内存映射 API
 * ======================================================================== */

/**
 * @brief 启用 mmap 内存映射
 *
 * @param pool 页池
 * @param mmap_size mmap 大小（字节）
 * @return 0 成功，-1 失败
 */
int vector_page_enable_mmap(vector_page_pool_t *pool, size_t mmap_size);

/**
 * @brief 禁用 mmap 内存映射
 *
 * @param pool 页池
 */
void vector_page_disable_mmap(vector_page_pool_t *pool);

/**
 * @brief 从 mmap 读取向量
 *
 * @param pool 页池
 * @param vector_id 向量 ID
 * @param out_vector 输出缓冲区
 * @return 0 成功，-1 失败
 */
int vector_page_get_vector_mmap(const vector_page_pool_t *pool,
                                int32_t vector_id,
                                float *out_vector);

/* ========================================================================
 * 页面完整性校验 API
 * ======================================================================== */

/**
 * @brief 设置校验和类型
 *
 * @param pool 页池
 * @param checksum_type 校验和类型（CRC32/ADLER32/NONE）
 */
void vector_page_set_checksum(vector_page_pool_t *pool, int checksum_type);

/**
 * @brief 验证页完整性
 *
 * @param pool 页池
 * @param page_id 页 ID
 * @return true 完整，false 损坏
 */
bool vector_page_verify(const vector_page_pool_t *pool, int32_t page_id);

/**
 * @brief 计算页校验和
 *
 * @param page 页
 * @param pool 页池（用于获取维度等信息）
 * @return 校验和值
 */
uint32_t vector_page_compute_checksum(const vector_page_t *page,
                                       const vector_page_pool_t *pool);

#ifdef __cplusplus
}
#endif

#endif /* VECTOR_PAGE_H */
