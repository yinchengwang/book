/*
 * lsh_multiprobe.h
 *
 * Multi-Probe LSH (多探测 LSH) 索引
 *
 * 功能：
 * - 基于标准 LSH 扩展，通过探测相邻桶提高召回率
 * - 使用探测序列生成算法（shifting-based）
 * - 支持 L2 距离（p-stable LSH）
 *
 * 相比标准 LSH 的优势：
 * - 更高的召回率（相同表数量下）
 * - 更少的哈希表（降低内存占用）
 * - 可配置的探测深度
 *
 * 使用示例：
 * @code
 *   lsh_multiprobe_t *idx = lsh_multiprobe_create(20, 4, 128);
 *   // n_hash=20, n_tables=4, dims=128
 *
 *   lsh_multiprobe_train(idx, n_train, train_vectors);
 *   lsh_multiprobe_add(idx, n, vectors, ids);
 *
 *   int result_ids[10];
 *   float result_dists[10];
 *   lsh_multiprobe_search(idx, query, 10, result_ids, result_dists, 10);
 *   // probes=10（探测 10 个相邻桶）
 *
 *   lsh_multiprobe_destroy(idx);
 * @endcode
 */

#ifndef DB_INDEX_VECTOR_LSH_MULTIPROBE_H
#define DB_INDEX_VECTOR_LSH_MULTIPROBE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */
typedef struct lsh_multiprobe lsh_multiprobe_t;

/* ── 核心 API ── */

/**
 * 创建 Multi-Probe LSH 索引
 * @param n_hash 每个表的哈希函数数量
 * @param n_tables 哈希表数量
 * @param dims 向量维度
 * @return 索引指针，失败返回 NULL
 */
lsh_multiprobe_t *lsh_multiprobe_create(int n_hash, int n_tables, int dims);

/**
 * 训练索引（生成哈希函数参数）
 * @param idx 索引指针
 * @param n 训练向量数量
 * @param vectors 训练向量数组 [n, dims]
 * @return 0 成功，-1 失败
 */
int lsh_multiprobe_train(lsh_multiprobe_t *idx, int n, const float *vectors);

/**
 * 添加向量
 * @param idx 索引指针
 * @param n 向量数量
 * @param vectors 向量数组 [n, dims]
 * @param ids 向量 ID 数组
 * @return 成功添加的数量
 */
int lsh_multiprobe_add(lsh_multiprobe_t *idx, int n, const float *vectors, const int *ids);

/**
 * 搜索 K 近邻
 * @param idx 索引指针
 * @param query 查询向量 [dims]
 * @param k 返回结果数量
 * @param ids 输出：匹配的向量 ID（需预分配 k 个）
 * @param distances 输出：对应的距离（需预分配 k 个）
 * @param n_probes 探测桶数量（包括主桶）
 * @return 实际返回结果数量
 */
int lsh_multiprobe_search(const lsh_multiprobe_t *idx, const float *query,
                          int k, int *ids, float *distances, int n_probes);

/**
 * 批量搜索
 * @param idx 索引指针
 * @param nq 查询数量
 * @param queries 查询向量数组 [nq, dims]
 * @param k 返回结果数量
 * @param ids 输出：匹配的向量 ID [nq, k]
 * @param distances 输出：对应的距离 [nq, k]
 * @param n_probes 探测桶数量
 * @return 0 成功，-1 失败
 */
int lsh_multiprobe_search_batch(const lsh_multiprobe_t *idx, int nq, const float *queries,
                                 int k, int *ids, float *distances, int n_probes);

/**
 * 获取索引统计信息
 * @param idx 索引指针
 * @param out_n_vectors 输出：向量数量
 * @param out_n_tables 输出：哈希表数量
 * @param out_dims 输出：向量维度
 */
void lsh_multiprobe_stats(const lsh_multiprobe_t *idx,
                           int *out_n_vectors, int *out_n_tables, int *out_dims);

/**
 * 保存索引到文件
 * @param idx 索引指针
 * @param path 文件路径
 * @return 0 成功，-1 失败
 */
int lsh_multiprobe_save(const lsh_multiprobe_t *idx, const char *path);

/**
 * 从文件加载索引
 * @param path 文件路径
 * @return 索引指针，失败返回 NULL
 */
lsh_multiprobe_t *lsh_multiprobe_load(const char *path);

/**
 * 销毁索引
 * @param idx 索引指针
 */
void lsh_multiprobe_destroy(lsh_multiprobe_t *idx);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_LSH_MULTIPROBE_H */