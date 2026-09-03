/**
 * @file streaming_index.h
 * @brief 流式向量索引 - 统一的流式插入接口
 *
 * 包装现有的 HNSW/DiskANN/IVF-PQ 索引，添加流式插入能力。
 * 提供统一的 API，隐藏内部复杂性。
 */

#ifndef STREAMING_INDEX_H
#define STREAMING_INDEX_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

typedef struct streaming_index streaming_index_t;
typedef struct faiss_hnsw faiss_hnsw_t;
typedef struct diskann diskann_t;

/* ========================================================================
 * 索引类型
 * ======================================================================== */

/**
 * @brief 流式索引支持的底层索引类型
 */
typedef enum {
    STREAMING_INDEX_HNSW = 0,      /**< HNSW */
    STREAMING_INDEX_DISKANN = 1,   /**< DiskANN */
    STREAMING_INDEX_IVF_PQ = 2,    /**< IVF-PQ */
} streaming_index_type_t;

/* ========================================================================
 * 流式插入配置
 * ======================================================================== */

/**
 * @brief 流式索引配置
 */
typedef struct streaming_index_config {
    streaming_index_type_t index_type;  /**< 底层索引类型 */

    /* 底层索引参数 */
    int32_t dims;                       /**< 向量维度 */
    int32_t M;                          /**< HNSW M 参数 */
    int32_t ef_construction;            /**< HNSW ef_construction */
    int32_t ef_search;                  /**< HNSW ef_search */

    /* 流式插入参数 */
    int32_t buffer_capacity;            /**< 缓冲区容量 */
    int32_t buffer_flush_threshold;     /**< 缓冲区刷写阈值 */
    int32_t merge_interval_ms;          /**< 合并间隔（毫秒） */
    bool enable_incremental_quant;      /**< 启用增量量化 */
    int32_t max_incremental_updates;    /**< 最大增量更新次数 */
} streaming_index_config_t;

/**
 * @brief 默认配置
 */
#define STREAMING_INDEX_DEFAULT_BUFFER_CAPACITY 10000
#define STREAMING_INDEX_DEFAULT_FLUSH_THRESHOLD 1000
#define STREAMING_INDEX_DEFAULT_MERGE_INTERVAL_MS 1000
#define STREAMING_INDEX_DEFAULT_MAX_INCREMENTAL_UPDATES 10

/**
 * @brief 创建默认配置
 */
streaming_index_config_t streaming_index_config_default(
    streaming_index_type_t type,
    int32_t dims);

/* ========================================================================
 * 流式索引操作
 * ======================================================================== */

/**
 * @brief 创建流式索引
 * @param config 配置
 * @return 索引句柄，失败返回 NULL
 */
streaming_index_t *streaming_index_create(const streaming_index_config_t *config);

/**
 * @brief 从现有索引创建流式索引
 *
 * 将现有的 HNSW/DiskANN 索引包装为流式索引。
 *
 * @param base_index 基础索引（HNSW 或 DiskANN）
 * @param type 索引类型
 * @param config 配置（可为 NULL）
 * @return 流式索引句柄，失败返回 NULL
 */
streaming_index_t *streaming_index_wrap(
    void *base_index,
    streaming_index_type_t type,
    const streaming_index_config_t *config);

/**
 * @brief 销毁流式索引
 * @param index 索引句柄
 */
void streaming_index_destroy(streaming_index_t *index);

/* ========================================================================
 * 插入操作
 * ======================================================================== */

/**
 * @brief 同步插入（阻塞）
 *
 * 立即将向量插入主索引，阻塞直到完成。
 *
 * @param index 索引句柄
 * @param vectors 向量数组
 * @param n 向量数量
 * @return 0 成功，-1 失败
 */
int32_t streaming_index_add(streaming_index_t *index, const float *vectors, int32_t n);

/**
 * @brief 流式插入（非阻塞）
 *
 * 将向量添加到缓冲区，立即返回。
 * 后台线程会在适当时机将缓冲区合并到主索引。
 *
 * @param index 索引句柄
 * @param vectors 向量数组
 * @param n 向量数量
 * @return 0 成功加入缓冲区，-1 失败
 */
int32_t streaming_index_streaming_add(streaming_index_t *index, const float *vectors, int32_t n);

/**
 * @brief 刷新缓冲区
 *
 * 强制将缓冲区中的数据合并到主索引。
 *
 * @param index 索引句柄
 * @return 0 成功，-1 失败
 */
int32_t streaming_index_flush(streaming_index_t *index);

/**
 * @brief 等待所有待处理的插入完成
 * @param index 索引句柄
 * @return 0 成功，-1 失败
 */
int32_t streaming_index_wait(streaming_index_t *index);

/* ========================================================================
 * 查询操作
 * ======================================================================== */

/**
 * @brief 搜索
 *
 * 同时搜索主索引和缓冲区，返回合并后的 Top-K 结果。
 *
 * @param index 索引句柄
 * @param query 查询向量
 * @param k 返回结果数
 * @param distances 输出距离数组（预分配 k）
 * @param ids 输出 ID 数组（预分配 k）
 * @return 实际返回结果数，-1 失败
 */
int32_t streaming_index_search(
    streaming_index_t *index,
    const float *query,
    int32_t k,
    float *distances,
    int32_t *ids);

/**
 * @brief 仅搜索主索引
 * @param index 索引句柄
 * @param query 查询向量
 * @param k 返回结果数
 * @param distances 输出距离数组
 * @param ids 输出 ID 数组
 * @return 实际返回结果数，-1 失败
 */
int32_t streaming_index_search_main(
    streaming_index_t *index,
    const float *query,
    int32_t k,
    float *distances,
    int32_t *ids);

/* ========================================================================
 * 统计信息
 * ======================================================================== */

/**
 * @brief 流式索引统计
 */
typedef struct streaming_index_stats {
    int32_t total_vectors;           /**< 总向量数 */
    int32_t main_index_vectors;      /**< 主索引中的向量数 */
    int32_t buffer_vectors;          /**< 缓冲区中的向量数 */
    int32_t pending_tasks;           /**< 待处理的合并任务数 */
    int32_t completed_merges;        /**< 已完成的合并次数 */
    int64_t total_vectors_merged;    /**< 累计合并向量数 */
    bool is_merging;                 /**< 是否正在合并 */
} streaming_index_stats_t;

/**
 * @brief 获取统计信息
 * @param index 索引句柄
 * @param stats 输出统计信息
 */
void streaming_index_get_stats(
    const streaming_index_t *index,
    streaming_index_stats_t *stats);

/**
 * @brief 获取主索引
 * @param index 索引句柄
 * @return 主索引指针（HNSW 或 DiskANN）
 */
void *streaming_index_get_main_index(streaming_index_t *index);

/**
 * @brief 获取向量总数
 * @param index 索引句柄
 * @return 向量总数
 */
int32_t streaming_index_size(const streaming_index_t *index);

#ifdef __cplusplus
}
#endif

#endif /* STREAMING_INDEX_H */
