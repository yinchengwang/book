/*
 * vector_index_c_api.h
 *
 * 向量索引 C API 导出层 - 供 Python ctypes 调用
 *
 * 注意: 此文件定义了统一的 C API 接口
 * 当前实现使用 Python 占位，待后续与项目内部索引集成
 */

#ifndef VECTOR_INDEX_C_API_H
#define VECTOR_INDEX_C_API_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 距离度量类型
 */
typedef enum {
    METRIC_L2 = 0,      /* 欧氏距离 */
    METRIC_IP = 1,      /* 内积/余弦相似度 */
    METRIC_COSINE = 2   /* 余弦距离 (需归一化) */
} metric_type_t;

/*
 * HNSW 索引 API
 */

/* 创建 HNSW 索引
 * @param dim 向量维度
 * @param M 每层最大连接数
 * @param ef_construction 构建时搜索范围
 * @param metric 距离度量类型
 * @return 索引句柄，失败返回 NULL
 */
void* hnsw_create(int32_t dim, int32_t M, int32_t ef_construction, int32_t metric);

/* 构建索引（批量）
 * @param index 索引句柄
 * @param n 向量数量
 * @param vectors 原始向量数据 (n * dim)
 * @return 成功添加的数量，失败返回 -1
 */
int32_t hnsw_build(void* index, int32_t n, const float* vectors);

/* 搜索 Top-K
 * @param index 索引句柄
 * @param query 查询向量
 * @param k 返回结果数量
 * @param ef_search 搜索时搜索范围
 * @param ids 输出: 结果索引
 * @param distances 输出: 距离值
 * @return 成功数量，失败返回 -1
 */
int32_t hnsw_search(void* index, const float* query, int32_t k, int32_t ef_search,
                    int32_t* ids, float* distances);

/* 批量搜索
 * @param index 索引句柄
 * @param queries 查询向量 (n_queries * dim)
 * @param n_queries 查询数量
 * @param k 返回结果数量
 * @param ef_search 搜索范围
 * @param ids 输出: 结果索引 (n_queries * k)
 * @param distances 输出: 距离值 (n_queries * k)
 * @return 成功数量
 */
int32_t hnsw_search_batch(void* index, const float* queries, int32_t n_queries,
                          int32_t k, int32_t ef_search,
                          int32_t* ids, float* distances);

/* 获取索引大小（字节）
 * @param index 索引句柄
 * @return 索引占用内存字节数
 */
int64_t hnsw_get_size(void* index);

/* 销毁索引
 * @param index 索引句柄
 */
void hnsw_destroy(void* index);

/*
 * IVF-PQ 索引 API
 */

/* 创建 IVF-PQ 索引
 * @param nlist 倒排列表数量
 * @param pq_m PQ 子空间数
 * @param pq_bits PQ 量化位数
 * @param dim 向量维度
 * @return 索引句柄
 */
void* ivf_pq_create(int32_t nlist, int32_t pq_m, int32_t pq_bits, int32_t dim);

/* 设置搜索探针数
 * @param index 索引句柄
 * @param nprobe 探针数
 */
void ivf_pq_set_nprobe(void* index, int32_t nprobe);

/* 训练索引
 * @param index 索引句柄
 * @param n 训练向量数量
 * @param vectors 训练数据
 * @return 成功返回 0
 */
int32_t ivf_pq_train(void* index, int32_t n, const float* vectors);

/* 添加向量
 * @param index 索引句柄
 * @param n 向量数量
 * @param vectors 向量数据
 * @param ids 向量 ID (可为空，使用顺序 ID)
 * @return 成功数量
 */
int32_t ivf_pq_add(void* index, int32_t n, const float* vectors, const int32_t* ids);

/* 搜索
 * @param index 索引句柄
 * @param query 查询向量
 * @param k 结果数量
 * @param ids 输出: 结果索引
 * @param distances 输出: 距离值
 * @return 成功数量
 */
int32_t ivf_pq_search(void* index, const float* query, int32_t k,
                      int32_t* ids, float* distances);

/* 批量搜索
 * @param index 索引句柄
 * @param queries 查询向量
 * @param n_queries 查询数量
 * @param k 结果数量
 * @param ids 输出
 * @param distances 输出
 * @return 成功数量
 */
int32_t ivf_pq_search_batch(void* index, const float* queries, int32_t n_queries,
                             int32_t k, int32_t* ids, float* distances);

/* 获取索引大小
 * @param index 索引句柄
 * @return 内存占用字节数
 */
int64_t ivf_pq_get_size(void* index);

/* 销毁索引
 * @param index 索引句柄
 */
void ivf_pq_destroy(void* index);

/*
 * LSH 索引 API
 */

/* 创建 LSH 索引
 * @param dim 向量维度
 * @param num_hash 哈希函数数量
 * @param table_size 哈希表大小
 * @return 索引句柄
 */
void* lsh_create(int32_t dim, int32_t num_hash, int32_t table_size);

/* 添加向量
 * @param index 索引句柄
 * @param n 向量数量
 * @param vectors 向量数据
 * @param ids 向量 ID
 * @return 成功数量
 */
int32_t lsh_add(void* index, int32_t n, const float* vectors, const int32_t* ids);

/* 搜索
 * @param index 索引句柄
 * @param query 查询向量
 * @param k 结果数量
 * @param ids 输出: 结果索引
 * @param distances 输出: 距离值
 * @return 成功数量
 */
int32_t lsh_search(void* index, const float* query, int32_t k,
                   int32_t* ids, float* distances);

/* 批量搜索
 * @param index 索引句柄
 * @param queries 查询向量
 * @param n_queries 查询数量
 * @param k 结果数量
 * @param ids 输出
 * @param distances 输出
 * @return 成功数量
 */
int32_t lsh_search_batch(void* index, const float* queries, int32_t n_queries,
                         int32_t k, int32_t* ids, float* distances);

/* 获取索引大小
 * @param index 索引句柄
 * @return 内存占用字节数
 */
int64_t lsh_get_size(void* index);

/* 销毁索引
 * @param index 索引句柄
 */
void lsh_destroy(void* index);

#ifdef __cplusplus
}
#endif

#endif /* VECTOR_INDEX_C_API_H */
