/**
 * @file ssg.h
 * @brief SSG (Synthesized Sparse Graph) 索引
 *
 * SSG 是 DiskANN 论文中提出的 Vamana 图变体，通过 Robust Prune 算法
 * 构建稀疏邻居图。相比 NSW，SSG 使用更严格的剪枝策略减少边数，
 * 提高搜索效率。
 *
 * 主要特性：
 * - 单层稀疏图结构
 * - Robust Prune 剪枝策略
 * - 长程边 + 短程边混合
 * - 支持内存和磁盘存储
 */

#ifndef DB_INDEX_VECTOR_INDEX_SSG_H
#define DB_INDEX_VECTOR_INDEX_SSG_H

#include <stdint.h>
#include "algo-prod/distance/distance.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
struct heap_vector_store;
typedef struct heap_vector_store heap_vector_store_t;

/**
 * @brief SSG 配置参数
 */
typedef struct {
    int32_t R;                  /* 每节点最大邻居数（推荐 16-64） */
    int32_t L;                  /* 构建搜索候选数（推荐 100-200） */
    int32_t max_search_L;       /* 搜索时最大候选数 */
    int32_t dims;               /* 向量维度 */
    float alpha;                /* Robust Prune 遮挡参数（推荐 1.2） */
    distance_metric_t metric;   /* 距离度量 */
} ssg_config_t;

/**
 * @brief SSG 节点
 */
typedef struct {
    int32_t id;                 /* 节点 ID */
    int32_t *neighbors;         /* 邻居数组 */
    int32_t n_neighbors;        /* 当前邻居数 */
    int32_t capacity;           /* 邻居数组容量 */
} ssg_node_t;

/**
 * @brief SSG 索引结构
 */
typedef struct {
    ssg_config_t config;        /* 配置参数 */
    ssg_node_t *nodes;          /* 节点数组 */
    int32_t n_nodes;            /* 当前节点数 */
    int32_t capacity;           /* 节点数组容量 */
    int32_t entry_point;        /* 入口点（冻结点） */

    /* 向量存储 */
    heap_vector_store_t *heap_store;
    int use_heap_store;         /* 是否使用 heap_store */
    int vectors_per_page_hint;  /* 每页向量数提示（用于 ref 计算） */

    /* 统计信息 */
    int64_t n_searches;         /* 搜索次数 */
    int64_t total_hops;         /* 总跳数 */
} ssg_index_t;

/* ========== 生命周期管理 ========== */

/**
 * @brief 创建 SSG 索引
 */
ssg_index_t *ssg_index_create(const ssg_config_t *config);

/**
 * @brief 销毁 SSG 索引
 */
void ssg_index_destroy(ssg_index_t *index);

/**
 * @brief 设置 heap 向量存储
 */
void ssg_index_set_heap_store(ssg_index_t *index, heap_vector_store_t *store);

/* ========== 向量操作 ========== */

/**
 * @brief 插入单个向量
 * @return 插入的向量 ID，失败返回 -1
 */
int32_t ssg_index_insert(ssg_index_t *index, const float *vector);

/**
 * @brief 批量插入向量
 * @return 成功插入的数量
 */
int32_t ssg_index_insert_batch(ssg_index_t *index, int32_t n_vectors,
                                const float *vectors);

/**
 * @brief 搜索 K 近邻
 * @return 找到的结果数量
 */
int32_t ssg_index_search(ssg_index_t *index, const float *query, int32_t k,
                         int32_t *result_ids, float *result_dists);

/* ========== 状态查询 ========== */

/**
 * @brief 获取索引中的向量数量
 */
int32_t ssg_index_size(const ssg_index_t *index);

/**
 * @brief 获取平均出度
 */
float ssg_index_avg_out_degree(const ssg_index_t *index);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_INDEX_SSG_H */