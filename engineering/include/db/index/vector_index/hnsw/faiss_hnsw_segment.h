/**
 * @file faiss_hnsw_segment.h
 * @brief faiss_hnsw 不可变 segment + 多段集合（C5.1-C5.3 COW 收敛）
 *
 * 段式抽象：在不修改 faiss_hnsw 内部的前提下提供 COW 写入能力。
 * - faiss_hnsw_segment：单个不可变 faiss_hnsw 索引
 * - faiss_hnsw_collection：多段集合，add 写入缓冲，超阈值触发 compact 创建新 segment
 */
#ifndef DB_FAISS_HNSW_SEGMENT_H
#define DB_FAISS_HNSW_SEGMENT_H

#include "db/index/vector_index/hnsw/faiss_hnsw.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct faiss_hnsw_segment_s {
    faiss_hnsw_t *index;        /**< 不可变 segment（外部 search 期间指针不变） */
    int64_t start_id;           /**< segment 起始 vector id */
    int64_t end_id;             /**< segment 结束 vector id */
    int64_t n_at_compact;       /**< compact 时该 segment 的向量数 */
} faiss_hnsw_segment_t;

typedef struct faiss_hnsw_collection_s {
    faiss_hnsw_segment_t *segs;       /**< 不可变 segment 数组（搜索时只读） */
    int32_t M;
    int32_t dims;
    int32_t metric;
    int32_t n_segs;                   /**< 当前 segment 数 */
    int32_t cap_segs;                 /**< segments[] 容量 */
    int32_t n_total;                  /**< 累计所有 segment 向量数 */
    /* 写缓冲：累积向量，超阈值后触发 compact */
    float *buf_vectors;               /**< 连续 float 数组 */
    int32_t *buf_ids;                 /**< 累计的 vector id */
    int32_t buf_count;                /**< 当前缓冲内向量数 */
    int32_t buf_cap;                  /**< 缓冲容量 */
    int32_t buf_threshold;            /**< 触发 compact 的阈值 */
    int64_t next_id;                  /**< 下一个分配的 vector id */
} faiss_hnsw_collection_t;

faiss_hnsw_collection_t *faiss_hnsw_collection_create(int32_t M, int32_t dims,
                                                       int32_t ef_construction,
                                                       distance_metric_t metric,
                                                       int32_t segment_threshold);
void faiss_hnsw_collection_destroy(faiss_hnsw_collection_t *col);

/* add：累积到缓冲；超阈值触发 compact */
int faiss_hnsw_collection_add(faiss_hnsw_collection_t *col,
                             int32_t n, const float *vectors);

/* search：遍历所有 segments + 缓冲，合并 top-k */
int32_t faiss_hnsw_collection_search(faiss_hnsw_collection_t *col,
                                     const float *query, int32_t k,
                                     float *distances, int32_t *ids);

/* ntotal：所有 segment + 缓冲的累计 */
int32_t faiss_hnsw_collection_ntotal(const faiss_hnsw_collection_t *col);

/* 主动 compact（不等到阈值） */
int faiss_hnsw_collection_compact(faiss_hnsw_collection_t *col);

#ifdef __cplusplus
}
#endif

#endif /* DB_FAISS_HNSW_SEGMENT_H */