/*
 * milvus_ivf.h
 *
 * Milvus IVF 风格索引
 *
 * 功能：
 * - 兼容 Milvus IVF 接口
 * - 支持 GPU 加速（预留接口）
 * - 优化的倒排索引
 *
 * 使用示例：
 * @code
 *   milvus_ivf_t *idx = milvus_ivf_create(128, 1024);
 *   // dims=128, nlist=1024
 *
 *   milvus_ivf_train(idx, n_train, train_vectors);
 *   milvus_ivf_add(idx, n, vectors, ids);
 *
 *   int result_ids[10];
 *   milvus_ivf_search(idx, query, 10, result_ids, 20);
 *
 *   milvus_ivf_destroy(idx);
 * @endcode
 */

#ifndef DB_INDEX_VECTOR_MILVUS_IVF_H
#define DB_INDEX_VECTOR_MILVUS_IVF_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */
typedef struct milvus_ivf milvus_ivf_t;

/* ── 核心 API ── */

/**
 * 创建 Milvus IVF 索引
 * @param dims 向量维度
 * @param nlist 簇数量
 * @return 索引指针，失败返回 NULL
 */
milvus_ivf_t *milvus_ivf_create(int dims, int nlist);

/**
 * 训练索引
 * @param idx 索引指针
 * @param n 训练向量数量
 * @param vectors 训练向量 [n, dims]
 * @return 0 成功，-1 失败
 */
int milvus_ivf_train(milvus_ivf_t *idx, int n, const float *vectors);

/**
 * 添加向量
 * @param idx 索引指针
 * @param n 向量数量
 * @param vectors 向量数组 [n, dims]
 * @param ids 向量 ID 数组
 * @return 成功添加的数量
 */
int milvus_ivf_add(milvus_ivf_t *idx, int n, const float *vectors, const int *ids);

/**
 * 搜索 K 近邻
 * @param idx 索引指针
 * @param query 查询向量 [dims]
 * @param k 返回结果数量
 * @param ids 输出：匹配的向量 ID
 * @param nprobe 探测的簇数量
 * @return 实际返回结果数量
 */
int milvus_ivf_search(const milvus_ivf_t *idx, const float *query,
                      int k, int *ids, int nprobe);

/**
 * 批量搜索
 * @param idx 索引指针
 * @param nq 查询数量
 * @param queries 查询向量数组 [nq, dims]
 * @param k 返回结果数量
 * @param ids 输出：匹配的向量 ID [nq, k]
 * @param nprobe 探测的簇数量
 * @return 0 成功，-1 失败
 */
int milvus_ivf_search_batch(const milvus_ivf_t *idx, int nq, const float *queries,
                            int k, int *ids, int nprobe);

/**
 * 获取统计信息
 * @param idx 索引指针
 * @param out_n_vectors 输出：向量数量
 * @param out_dims 输出：维度
 * @param out_nlist 输出：簇数量
 */
void milvus_ivf_stats(const milvus_ivf_t *idx,
                      int *out_n_vectors, int *out_dims, int *out_nlist);

/**
 * 保存索引
 * @param idx 索引指针
 * @param path 文件路径
 * @return 0 成功，-1 失败
 */
int milvus_ivf_save(const milvus_ivf_t *idx, const char *path);

/**
 * 加载索引
 * @param path 文件路径
 * @return 索引指针，失败返回 NULL
 */
milvus_ivf_t *milvus_ivf_load(const char *path);

/**
 * 销毁索引
 * @param idx 索引指针
 */
void milvus_ivf_destroy(milvus_ivf_t *idx);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_MILVUS_IVF_H */