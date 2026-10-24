/*
 * faiss_ivf_compat.h
 *
 * Faiss IVF 兼容层
 *
 * 功能：
 * - 兼容 Faiss IVF API
 * - 支持模型导入导出
 * - 高效的倒排索引
 *
 * 使用示例：
 * @code
 *   faiss_ivf_compat_t *idx = faiss_ivf_compat_create(128, 1024);
 *
 *   faiss_ivf_compat_train(idx, n_train, train_vectors);
 *   faiss_ivf_compat_add(idx, n, vectors, ids);
 *
 *   int result_ids[10];
 *   float result_dists[10];
 *   faiss_ivf_compat_search(idx, query, 10, result_ids, result_dists, 20);
 *
 *   faiss_ivf_compat_destroy(idx);
 * @endcode
 */

#ifndef DB_INDEX_VECTOR_FAISS_IVF_COMPAT_H
#define DB_INDEX_VECTOR_FAISS_IVF_COMPAT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */
typedef struct faiss_ivf_compat faiss_ivf_compat_t;

/* ── 核心 API ── */

/**
 * 创建 Faiss IVF 兼容索引
 * @param dims 向量维度
 * @param nlist 簇数量
 * @return 索引指针，失败返回 NULL
 */
faiss_ivf_compat_t *faiss_ivf_compat_create(int dims, int nlist);

/**
 * 训练索引
 * @param idx 索引指针
 * @param n 训练向量数量
 * @param vectors 训练向量 [n, dims]
 * @return 0 成功，-1 失败
 */
int faiss_ivf_compat_train(faiss_ivf_compat_t *idx, int n, const float *vectors);

/**
 * 添加向量
 * @param idx 索引指针
 * @param n 向量数量
 * @param vectors 向量数组 [n, dims]
 * @param ids 向量 ID 数组
 * @return 成功添加的数量
 */
int faiss_ivf_compat_add(faiss_ivf_compat_t *idx, int n, const float *vectors, const int *ids);

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
int faiss_ivf_compat_search(const faiss_ivf_compat_t *idx, const float *query,
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
int faiss_ivf_compat_search_batch(const faiss_ivf_compat_t *idx, int nq, const float *queries,
                                  int k, int *ids, float *distances, int nprobe);

/**
 * 获取统计信息
 * @param idx 索引指针
 * @param out_n_vectors 输出：向量数量
 * @param out_dims 输出：维度
 * @param out_nlist 输出：簇数量
 */
void faiss_ivf_compat_stats(const faiss_ivf_compat_t *idx,
                            int *out_n_vectors, int *out_dims, int *out_nlist);

/**
 * 保存索引
 * @param idx 索引指针
 * @param path 文件路径
 * @return 0 成功，-1 失败
 */
int faiss_ivf_compat_save(const faiss_ivf_compat_t *idx, const char *path);

/**
 * 加载索引
 * @param path 文件路径
 * @return 索引指针，失败返回 NULL
 */
faiss_ivf_compat_t *faiss_ivf_compat_load(const char *path);

/**
 * 销毁索引
 * @param idx 索引指针
 */
void faiss_ivf_compat_destroy(faiss_ivf_compat_t *idx);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_FAISS_IVF_COMPAT_H */