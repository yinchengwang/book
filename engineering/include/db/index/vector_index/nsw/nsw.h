/**
 * @file nsw.h
 * @brief NSW（Navigable Small World）单层图索引
 *
 * NSW 是 HNSW 的前身算法，使用单层可导航小世界图结构。
 * 本实现作为图索引的基础组件，提供 O(log n) 搜索复杂度。
 */

#ifndef NSW_INDEX_H
#define NSW_INDEX_H

#include <stdint.h>
#include <stdbool.h>

#include <algo-prod/distance/distance.h>
#include <db/index/vector_ref.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct heap_vector_store heap_vector_store_t;

/**
 * @brief NSW 索引配置
 */
typedef struct nsw_config {
    int32_t M;              /* 每个节点的最大邻居数 */
    int32_t ef_construction;/* 构建时的候选队列大小 */
    int32_t ef_search;      /* 搜索时的候选队列大小 */
    int32_t dims;           /* 向量维度 */
    distance_metric_t metric; /* 距离度量 */
} nsw_config_t;

/**
 * @brief NSW 节点结构
 */
typedef struct nsw_node {
    int32_t id;             /* 节点 ID */
    int32_t *neighbors;     /* 邻居节点 ID 数组 */
    int32_t n_neighbors;    /* 当前邻居数量 */
    int32_t max_neighbors;  /* 最大邻居数（= M） */
} nsw_node_t;

/**
 * @brief NSW 索引结构
 */
typedef struct nsw_index {
    /* 配置参数 */
    nsw_config_t config;

    /* 图结构 */
    nsw_node_t *nodes;          /* 节点数组 */
    int32_t n_nodes;            /* 当前节点数 */
    int32_t max_nodes;          /* 最大节点容量 */
    int32_t entry_point;        /* 入口节点 ID */

    /* 向量存储 */
    float *vectors;             /* 向量数据（兼容模式） */
    heap_vector_store_t *heap_store; /* Heap 存储（优先模式） */
    vector_ref_t *vector_refs;  /* 向量引用数组 */
    uint32_t vector_refs_size;  /* 引用数组大小 */
} nsw_index_t;

/**
 * @brief 创建 NSW 索引
 * @param config 配置参数
 * @return 索引指针，失败返回 NULL
 */
nsw_index_t *nsw_index_create(const nsw_config_t *config);

/**
 * @brief 销毁 NSW 索引
 * @param index 索引指针
 */
void nsw_index_destroy(nsw_index_t *index);

/**
 * @brief 插入向量
 * @param index 索引指针
 * @param vector 向量数据
 * @return 插入的节点 ID，失败返回 -1
 */
int32_t nsw_index_insert(nsw_index_t *index, const float *vector);

/**
 * @brief 批量插入向量
 * @param index 索引指针
 * @param n 向量数量
 * @param vectors 向量数据
 * @return 成功插入的数量
 */
int32_t nsw_index_insert_batch(nsw_index_t *index, int32_t n, const float *vectors);

/**
 * @brief 搜索 k 最近邻
 * @param index 索引指针
 * @param query 查询向量
 * @param k 返回数量
 * @param result_ids 结果 ID 数组
 * @param result_dists 结果距离数组
 * @return 实际返回数量
 */
int32_t nsw_index_search(const nsw_index_t *index,
                         const float *query,
                         int32_t k,
                         int32_t *result_ids,
                         float *result_dists);

/**
 * @brief 设置 Heap 向量存储
 * @param index 索引指针
 * @param heap_store Heap 存储指针
 * @return 0 成功，-1 失败
 */
int32_t nsw_index_set_heap_store(nsw_index_t *index,
                                  heap_vector_store_t *heap_store);

/**
 * @brief 获取索引中的向量数量
 * @param index 索引指针
 * @return 向量数量
 */
int32_t nsw_index_size(const nsw_index_t *index);

#ifdef __cplusplus
}
#endif

#endif /* NSW_INDEX_H */
