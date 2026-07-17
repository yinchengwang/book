/*
 * hnsw_sq.h
 *
 * HNSW-SQ 混合索引
 *
 * 功能：
 * - HNSW 图搜索 + SQ 标量量化
 * - 内存优化的近似搜索
 * - 重排序提高精度
 *
 * 原理：
 * 1. SQ 量化：将每个维度压缩到 8 位
 * 2. HNSW 图：在量化空间构建多层图
 * 3. 搜索：量化距离初筛 + 精确距离重排
 *
 * 使用示例：
 * @code
 *   hnsw_sq_t *idx = hnsw_sq_create(128, 16, 64);
 *   // dims=128, M=16, ef_construction=64
 *
 *   hnsw_sq_train(idx, n_train, train_vectors);
 *   hnsw_sq_add(idx, n, vectors, ids);
 *
 *   int result_ids[10];
 *   float result_dists[10];
 *   hnsw_sq_search(idx, query, 10, result_ids, result_dists);
 *
 *   hnsw_sq_destroy(idx);
 * @endcode
 */

#ifndef DB_INDEX_VECTOR_HNSW_SQ_H
#define DB_INDEX_VECTOR_HNSW_SQ_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */
typedef struct hnsw_sq hnsw_sq_t;

/* ── 核心 API ── */

/**
 * 创建 HNSW-SQ 索引
 * @param dims 向量维度
 * @param M 图连接度
 * @param ef_construction 构建时的候选集大小
 * @return 索引指针，失败返回 NULL
 */
hnsw_sq_t *hnsw_sq_create(int dims, int M, int ef_construction);

/**
 * 训练索引
 * @param idx 索引指针
 * @param n 训练向量数量
 * @param vectors 训练向量 [n, dims]
 * @return 0 成功，-1 失败
 */
int hnsw_sq_train(hnsw_sq_t *idx, int n, const float *vectors);

/**
 * 添加向量
 * @param idx 索引指针
 * @param n 向量数量
 * @param vectors 向量数组 [n, dims]
 * @param ids 向量 ID 数组
 * @return 成功添加的数量
 */
int hnsw_sq_add(hnsw_sq_t *idx, int n, const float *vectors, const int *ids);

/**
 * 搜索 K 近邻
 * @param idx 索引指针
 * @param query 查询向量 [dims]
 * @param k 返回结果数量
 * @param ids 输出：匹配的向量 ID
 * @param distances 输出：对应的距离
 * @return 实际返回结果数量
 */
int hnsw_sq_search(const hnsw_sq_t *idx, const float *query,
                  int k, int *ids, float *distances);

/**
 * 获取统计信息
 * @param idx 索引指针
 * @param out_n_vectors 输出：向量数量
 * @param out_dims 输出：维度
 */
void hnsw_sq_stats(const hnsw_sq_t *idx, int *out_n_vectors, int *out_dims);

/**
 * 保存索引
 * @param idx 索引指针
 * @param path 文件路径
 * @return 0 成功，-1 失败
 */
int hnsw_sq_save(const hnsw_sq_t *idx, const char *path);

/**
 * 加载索引
 * @param path 文件路径
 * @return 索引指针，失败返回 NULL
 */
hnsw_sq_t *hnsw_sq_load(const char *path);

/**
 * 销毁索引
 * @param idx 索引指针
 */
void hnsw_sq_destroy(hnsw_sq_t *idx);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_HNSW_SQ_H */