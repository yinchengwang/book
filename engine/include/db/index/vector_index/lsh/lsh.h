/*
 * lsh.h
 *
 * LSH (Locality-Sensitive Hashing) 局部敏感哈希索引
 *
 * 功能：
 * - 支持二值向量和浮点向量的近似最近邻搜索
 * - 提供多种 LSH 类型：Bitwise、p-stable、SimHash
 *
 * 使用示例：
 * @code
 *   lsh_index_t *idx = lsh_create(LSH_PSTABLE, 20, 8, 128);
 *   // n_hash=20, n_tables=8, dims=128
 *
 *   lsh_train(idx, n_train, train_vectors);
 *   lsh_add(idx, n, vectors, ids);
 *
 *   int result_ids[10];
 *   float result_dists[10];
 *   lsh_search(idx, query, 10, result_ids, result_dists);
 *
 *   lsh_destroy(idx);
 * @endcode
 */

#ifndef DB_INDEX_VECTOR_LSH_H
#define DB_INDEX_VECTOR_LSH_H

#include <stdbool.h>
#include <stdint.h>

#include <db/index/storage_backend.h>
#include <db/index/heap/heap_vector_store.h>
#include <db/index/vector_ref.h>
#include <db/index/index_persist_control.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */
typedef struct lsh_index lsh_index_t;

/* Phase 1 基础设施前向声明 */
typedef struct storage_backend storage_backend_t;
typedef struct heap_vector_store heap_vector_store_t;
typedef struct persist_control persist_control_t;
typedef struct vector_ref vector_ref_t;

/* ── LSH 类型 ── */
typedef enum lsh_type {
    LSH_BITWISE = 0,     /* 比特采样 LSH（汉明距离） */
    LSH_PSTABLE = 1,     /* p-stable LSH（L2 距离） */
    LSH_SIMHASH = 2,     /* SimHash（Cosine 相似度） */
} lsh_type_t;

/* ── 核心 API ── */

/**
 * 创建 LSH 索引
 * @param type LSH 类型
 * @param n_hash 每个表的哈希函数数量
 * @param n_tables 哈希表数量
 * @param dims 向量维度
 * @return LSH 索引指针，失败返回 NULL
 */
lsh_index_t *lsh_create(lsh_type_t type, int n_hash, int n_tables, int dims);

/**
 * 训练 LSH（生成哈希函数参数）
 * @param idx LSH 索引
 * @param n 训练向量数量
 * @param vectors 训练向量数组 [n, dims]
 * @return 0 成功，-1 失败
 */
int lsh_train(lsh_index_t *idx, int n, const float *vectors);

/**
 * 添加向量
 * @param idx LSH 索引
 * @param n 向量数量
 * @param vectors 向量数组 [n, dims]
 * @param ids 向量 ID 数组
 * @return 成功添加的数量
 */
int lsh_add(lsh_index_t *idx, int n, const float *vectors, const int *ids);

/**
 * 添加二值向量
 * @param idx LSH 索引
 * @param n 向量数量
 * @param vectors 二值向量数组（每 bit 一个维度）
 * @param ids 向量 ID 数组
 * @return 成功添加的数量
 */
int lsh_add_binary(lsh_index_t *idx, int n, const uint8_t *vectors, const int *ids);

/**
 * 搜索 K 近邻
 * @param idx LSH 索引
 * @param query 查询向量 [dims]
 * @param k 返回结果数量
 * @param ids 输出：匹配的向量 ID（需预分配 k 个）
 * @param distances 输出：对应的距离（需预分配 k 个）
 * @return 实际返回结果数量
 */
int lsh_search(const lsh_index_t *idx, const float *query, int k, int *ids, float *distances);

/**
 * 批量搜索
 * @param idx LSH 索引
 * @param nq 查询数量
 * @param queries 查询向量数组 [nq, dims]
 * @param k 返回结果数量
 * @param ids 输出：匹配的向量 ID [nq, k]
 * @param distances 输出：对应的距离 [nq, k]
 * @return 0 成功，-1 失败
 */
int lsh_search_batch(const lsh_index_t *idx, int nq, const float *queries,
                     int k, int *ids, float *distances);

/**
 * 获取索引统计信息
 * @param idx LSH 索引
 * @param out_n_vectors 输出：向量数量
 * @param out_n_tables 输出：哈希表数量
 * @param out_dims 输出：向量维度
 */
void lsh_stats(const lsh_index_t *idx, int *out_n_vectors, int *out_n_tables, int *out_dims);

/**
 * 保存索引到文件
 * @param idx LSH 索引
 * @param path 文件路径
 * @return 0 成功，-1 失败
 */
int lsh_save(const lsh_index_t *idx, const char *path);

/**
 * 从文件加载索引
 * @param path 文件路径
 * @return LSH 索引指针，失败返回 NULL
 */
lsh_index_t *lsh_load(const char *path);

/**
 * 销毁 LSH 索引
 * @param idx LSH 索引
 */
void lsh_destroy(lsh_index_t *idx);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_LSH_H */
