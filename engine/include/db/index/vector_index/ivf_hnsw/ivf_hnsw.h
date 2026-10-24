/*
 * ivf_hnsw.h
 *
 * IVF-HNSW 混合索引
 *
 * 功能：
 * - IVF 倒排索引 + HNSW 图搜索
 * - 先聚类定位，再图搜索
 * - 高召回率 + 高效率
 *
 * 原理：
 * 1. IVF 聚类：将向量分配到最近的簇
 * 2. HNSW 图：在每个簇内构建图
 * 3. 搜索：定位最近簇 → 图搜索
 */

#ifndef DB_INDEX_VECTOR_IVF_HNSW_H
#define DB_INDEX_VECTOR_IVF_HNSW_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */
typedef struct ivf_hnsw ivf_hnsw_t;

/* ── 核心 API ── */

/**
 * 创建 IVF-HNSW 索引
 * @param dims 向量维度
 * @param nlist 簇数量
 * @param M HNSW 连接度
 * @param ef_construction HNSW 构建参数
 * @return 索引指针，失败返回 NULL
 */
ivf_hnsw_t *ivf_hnsw_create(int dims, int nlist, int M, int ef_construction);

/**
 * 训练索引
 * @param idx 索引指针
 * @param n 训练向量数量
 * @param vectors 训练向量 [n, dims]
 * @return 0 成功，-1 失败
 */
int ivf_hnsw_train(ivf_hnsw_t *idx, int n, const float *vectors);

/**
 * 添加向量
 * @param idx 索引指针
 * @param n 向量数量
 * @param vectors 向量数组 [n, dims]
 * @param ids 向量 ID 数组
 * @return 成功添加的数量
 */
int ivf_hnsw_add(ivf_hnsw_t *idx, int n, const float *vectors, const int *ids);

/**
 * 搜索 K 近邻
 * @param idx 索引指针
 * @param query 查询向量 [dims]
 * @param k 返回结果数量
 * @param ids 输出：匹配的向量 ID
 * @param distances 输出：对应的距离
 * @param nprobe 探测的簇数量
 * @return 实际返回结果数量
 */
int ivf_hnsw_search(const ivf_hnsw_t *idx, const float *query,
                   int k, int *ids, float *distances, int nprobe);

/**
 * 批量搜索
 * @param idx 索引指针
 * @param nq 查询数量
 * @param queries 查询向量数组 [nq, dims]
 * @param k 返回结果数量
 * @param ids 输出：匹配的向量 ID [nq, k]
 * @param distances 输出：对应的距离 [nq, k]
 * @param nprobe 探测的簇数量
 * @return 0 成功，-1 失败
 */
int ivf_hnsw_search_batch(const ivf_hnsw_t *idx, int nq, const float *queries,
                          int k, int *ids, float *distances, int nprobe);

/**
 * 获取统计信息
 * @param idx 索引指针
 * @param out_n_vectors 输出：向量数量
 * @param out_dims 输出：维度
 */
void ivf_hnsw_stats(const ivf_hnsw_t *idx, int *out_n_vectors, int *out_dims);

/**
 * 保存索引
 * @param idx 索引指针
 * @param path 文件路径
 * @return 0 成功，-1 失败
 */
int ivf_hnsw_save(const ivf_hnsw_t *idx, const char *path);

/**
 * 加载索引
 * @param path 文件路径
 * @return 索引指针，失败返回 NULL
 */
ivf_hnsw_t *ivf_hnsw_load(const char *path);

/**
 * 销毁索引
 * @param idx 索引指针
 */
void ivf_hnsw_destroy(ivf_hnsw_t *idx);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_IVF_HNSW_H */