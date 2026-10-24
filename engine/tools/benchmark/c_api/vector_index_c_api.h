/*
 * vector_index_c_api.h
 *
 * 向量索引 C API 导出层 - 供 Python ctypes 调用
 */

#ifndef VECTOR_INDEX_C_API_H
#define VECTOR_INDEX_C_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * HNSW 索引 API
 * ============================================================================ */

/**
 * @brief 创建 HNSW 索引上下文
 * @param dim 向量维度
 * @param M 每层最大邻居数
 * @param ef_construction 构建时的 ef 参数
 * @param metric 距离度量: 0=L2, 1=IP
 * @return 索引上下文句柄，失败返回 NULL
 */
void* hnsw_create(int32_t dim, int32_t M, int32_t ef_construction, int32_t metric);

/**
 * @brief 向 HNSW 索引添加向量
 * @param ctx 索引上下文
 * @param n 向量数量
 * @param vectors 向量数据 (n * dim 个 float)
 * @return 成功添加的数量
 */
int32_t hnsw_build(void* ctx, int32_t n, const float* vectors);

/**
 * @brief 搜索 HNSW 索引
 * @param ctx 索引上下文
 * @param query 查询向量 (dim 个 float)
 * @param k 返回 Top-K 结果
 * @param ef_search 搜索时的 ef 参数
 * @param ids 输出: Top-K 向量 ID
 * @param distances 输出: Top-K 距离
 * @return 实际返回结果数量
 */
int32_t hnsw_search(void* ctx, const float* query, int32_t k, int32_t ef_search,
                    int32_t* ids, float* distances);

/**
 * @brief 批量搜索 HNSW 索引
 */
int32_t hnsw_search_batch(void* ctx, const float* queries, int32_t n_queries,
                          int32_t k, int32_t ef_search,
                          int32_t* ids, float* distances);

/**
 * @brief 获取索引内存大小
 */
int64_t hnsw_get_size(void* ctx);

/**
 * @brief 销毁 HNSW 索引上下文
 */
void hnsw_destroy(void* ctx);

/* ============================================================================
 * IVF-PQ 索引 API (映射到 DiskANN 实现)
 * ============================================================================ */

/**
 * @brief 创建 IVF-PQ 索引上下文
 */
void* ivf_pq_create(int32_t nlist, int32_t pq_m, int32_t pq_bits, int32_t dim);

void ivf_pq_set_nprobe(void* ctx, int32_t nprobe);

int32_t ivf_pq_train(void* ctx, int32_t n, const float* vectors);

int32_t ivf_pq_add(void* ctx, int32_t n, const float* vectors, const int32_t* ids);

int32_t ivf_pq_search(void* ctx, const float* query, int32_t k,
                      int32_t* ids, float* distances);

int32_t ivf_pq_search_batch(void* ctx, const float* queries, int32_t n_queries,
                             int32_t k, int32_t* ids, float* distances);

int64_t ivf_pq_get_size(void* ctx);

void ivf_pq_destroy(void* ctx);

/* ============================================================================
 * LSH 索引 API
 * ============================================================================ */

/**
 * @brief 创建 LSH 索引上下文
 * @param dim 向量维度
 * @param num_hash 哈希函数数量
 * @param table_size 哈希表大小
 */
void* lsh_create(int32_t dim, int32_t num_hash, int32_t table_size);

int32_t lsh_add(void* ctx, int32_t n, const float* vectors, const int32_t* ids);

int32_t lsh_search(void* ctx, const float* query, int32_t k,
                   int32_t* ids, float* distances);

int32_t lsh_search_batch(void* ctx, const float* queries, int32_t n_queries,
                         int32_t k, int32_t* ids, float* distances);

int64_t lsh_get_size(void* ctx);

void lsh_destroy(void* ctx);

#ifdef __cplusplus
}
#endif

#endif /* VECTOR_INDEX_C_API_H */
