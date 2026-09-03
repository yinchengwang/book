/**
 * @file hnsw_pq.h
 * @brief HNSW-PQ 混合索引
 *
 * 结合 HNSW 图结构和 PQ 量化的混合索引：
 * - 图节点存储 PQ 编码（压缩）
 * - 导航使用 ADC 距离（近似）
 * - 重排序使用精确距离计算
 *
 * 优势：
 * - 内存占用低（压缩比 32x）
 * - 搜索速度接近 HNSW
 * - 适合大规模数据集
 */

#ifndef DB_INDEX_VECTOR_HNSW_PQ_H
#define DB_INDEX_VECTOR_HNSW_PQ_H

#include <stdint.h>
#include "algo-prod/distance/distance.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 - 直接包含 PQ 头文件 */
#include "db/index/vector_index/pq/pq.h"

typedef struct heap_vector_store heap_vector_store_t;

/**
 * @brief HNSW-PQ 配置
 */
typedef struct {
    int m;                  /* HNSW 最大邻居数 */
    int ef_construction;    /* 构建时候选数 */
    int ef_search;          /* 搜索时候选数 */
    int dims;               /* 向量维度 */
    distance_metric_t metric;

    /* PQ 参数 */
    int pq_m;               /* PQ 子空间数 */
    int pq_bits;            /* PQ 每子空间比特数 */
    int rerank_k;           /* 重排序候选数 */
} hnsw_pq_config_t;

/**
 * @brief HNSW-PQ 节点
 */
typedef struct {
    int32_t id;             /* 节点 ID */
    int32_t *neighbors;     /* 邻居数组 */
    int32_t n_neighbors;    /* 当前邻居数 */
    uint8_t *pq_code;       /* PQ 编码 */
} hnsw_pq_node_t;

/**
 * @brief HNSW-PQ 索引结构
 */
typedef struct {
    hnsw_pq_config_t config;
    hnsw_pq_node_t *nodes;  /* 节点数组 */
    int32_t n_nodes;        /* 当前节点数 */
    int32_t capacity;       /* 节点数组容量 */
    int32_t entry_point;    /* 入口点 */

    pq_quantizer_t *pq;     /* PQ 量化器 */
    heap_vector_store_t *heap_store;  /* 原始向量存储（用于重排序） */
    int use_heap_store;

    /* 统计信息 */
    int64_t n_searches;
    int64_t total_hops;
    int64_t total_reranks;
} hnsw_pq_index_t;

/* ========== 生命周期管理 ========== */

/**
 * @brief 创建 HNSW-PQ 索引
 */
hnsw_pq_index_t *hnsw_pq_index_create(const hnsw_pq_config_t *config);

/**
 * @brief 销毁 HNSW-PQ 索引
 */
void hnsw_pq_index_destroy(hnsw_pq_index_t *index);

/**
 * @brief 设置 PQ 量化器
 */
void hnsw_pq_index_set_pq(hnsw_pq_index_t *index, pq_quantizer_t *pq);

/**
 * @brief 设置 heap_store（用于重排序）
 */
void hnsw_pq_index_set_heap_store(hnsw_pq_index_t *index, heap_vector_store_t *store);

/* ========== 向量操作 ========== */

/**
 * @brief 训练 PQ 码本
 * @param n 训练向量数量
 * @param vectors 训练向量数组
 * @return 0 成功，-1 失败
 */
int hnsw_pq_index_train(hnsw_pq_index_t *index, int n, const float *vectors);

/**
 * @brief 插入单个向量
 * @return 插入的向量 ID，失败返回 -1
 */
int32_t hnsw_pq_index_insert(hnsw_pq_index_t *index, const float *vector);

/**
 * @brief 批量插入向量
 * @return 成功插入的数量
 */
int32_t hnsw_pq_index_insert_batch(hnsw_pq_index_t *index, int n, const float *vectors);

/* ========== 搜索操作 ========== */

/**
 * @brief 搜索 K 近邻（带重排序）
 * @param query 查询向量
 * @param k 返回数量
 * @param result_ids 结果 ID 数组
 * @param result_dists 结果距离数组
 * @return 找到的结果数量
 */
int32_t hnsw_pq_index_search(hnsw_pq_index_t *index, const float *query,
                              int32_t k, int32_t *result_ids, float *result_dists);

/* ========== 状态查询 ========== */

/**
 * @brief 获取索引中的向量数量
 */
int32_t hnsw_pq_index_size(const hnsw_pq_index_t *index);

/**
 * @brief 获取平均出度
 */
float hnsw_pq_index_avg_out_degree(const hnsw_pq_index_t *index);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_HNSW_PQ_H */