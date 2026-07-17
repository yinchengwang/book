/*
 * scann.h
 *
 * ScaNN (Scalable Nearest Neighbors) 索引
 *
 * 功能：
 * - 高性能向量检索（Google 实现）
 * - 量化 + 图混合架构
 * - 各向同性哈希（anisotropic quantization）
 *
 * 特点：
 * - 内积距离优化（比 HNSW 更快）
 * - 支持重排序提高精度
 * - 内存效率高
 *
 * 使用示例：
 * @code
 *   scann_t *idx = scann_create(128, 10);
 *   // dims=128, n_neighbors=10
 *
 *   scann_train(idx, n_train, train_vectors);
 *   scann_add(idx, n, vectors, ids);
 *
 *   int result_ids[10];
 *   float result_dists[10];
 *   scann_search(idx, query, 10, result_ids, result_dists);
 *
 *   scann_destroy(idx);
 * @endcode
 */

#ifndef DB_INDEX_VECTOR_SCANN_H
#define DB_INDEX_VECTOR_SCANN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */
typedef struct scann scann_t;

/* ── 核心 API ── */

/**
 * 创建 ScaNN 索引
 * @param dims 向量维度
 * @param n_neighbors 图连接度
 * @return 索引指针，失败返回 NULL
 */
scann_t *scann_create(int dims, int n_neighbors);

/**
 * 训练索引（学习量化器）
 * @param idx 索引指针
 * @param n 训练向量数量
 * @param vectors 训练向量数组 [n, dims]
 * @return 0 成功，-1 失败
 */
int scann_train(scann_t *idx, int n, const float *vectors);

/**
 * 添加向量
 * @param idx 索引指针
 * @param n 向量数量
 * @param vectors 向量数组 [n, dims]
 * @param ids 向量 ID 数组
 * @return 成功添加的数量
 */
int scann_add(scann_t *idx, int n, const float *vectors, const int *ids);

/**
 * 搜索 K 近邻
 * @param idx 索引指针
 * @param query 查询向量 [dims]
 * @param k 返回结果数量
 * @param ids 输出：匹配的向量 ID（需预分配 k 个）
 * @param distances 输出：对应的距离（需预分配 k 个）
 * @return 实际返回结果数量
 */
int scann_search(const scann_t *idx, const float *query,
                 int k, int *ids, float *distances);

/**
 * 批量搜索
 * @param idx 索引指针
 * @param nq 查询数量
 * @param queries 查询向量数组 [nq, dims]
 * @param k 返回结果数量
 * @param ids 输出：匹配的向量 ID [nq, k]
 * @param distances 输出：对应的距离 [nq, k]
 * @return 0 成功，-1 失败
 */
int scann_search_batch(const scann_t *idx, int nq, const float *queries,
                       int k, int *ids, float *distances);

/**
 * 获取索引统计信息
 * @param idx 索引指针
 * @param out_n_vectors 输出：向量数量
 * @param out_n_neighbors 输出：图连接度
 * @param out_dims 输出：向量维度
 */
void scann_stats(const scann_t *idx,
                 int *out_n_vectors, int *out_n_neighbors, int *out_dims);

/**
 * 保存索引到文件
 * @param idx 索引指针
 * @param path 文件路径
 * @return 0 成功，-1 失败
 */
int scann_save(const scann_t *idx, const char *path);

/**
 * 从文件加载索引
 * @param path 文件路径
 * @return 索引指针，失败返回 NULL
 */
scann_t *scann_load(const char *path);

/**
 * 销毁索引
 * @param idx 索引指针
 */
void scann_destroy(scann_t *idx);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_SCANN_H */