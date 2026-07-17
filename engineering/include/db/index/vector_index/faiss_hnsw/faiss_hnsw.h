// faiss_hnsw.h
// 内存版 HNSW 索引（参考 FAISS HNSW.cpp 重写）
// 提供基于 Hierarchical Navigable Small World 图的近似最近邻搜索

#ifndef FAISS_HNSW_H
#define FAISS_HNSW_H

#include <stdint.h>
#include <stdbool.h>
#include <algo-prod/distance/distance.h>
#include <algo-prod/quantization/quantization.h>

#ifdef __cplusplus
extern "C" {
#endif

// 不透明的 HNSW 索引结构体类型
// 实际定义在 faiss_hnsw.c 中，对外仅暴露指针
typedef struct faiss_hnsw faiss_hnsw_t;

/**
 * 创建 HNSW 索引实例
 *
 * @param M               每个节点在每层的邻居数（典型值 16-32）
 * @param dims            向量维度
 * @param ef_construction 构建时的搜索宽度（典型值 64-200，越大越精确但越慢）
 * @param metric          距离度量类型（L2 欧氏距离或 IP 内积）
 * @param quant_type      量化类型（FP32 / SQ8 / PQ 等）
 * @return                索引指针，失败返回 NULL
 */
faiss_hnsw_t *faiss_hnsw_index_create(int32_t M, int32_t dims, int32_t ef_construction,
                                       distance_metric_t metric, quantization_type_t quant_type);

/**
 * 向索引中添加向量
 *
 * @param index   索引指针
 * @param n       向量数量
 * @param vectors 向量数据，行主序布局，长度 n*dims
 * @return        实际添加的向量数（成功等于 n），失败返回负数
 */
int32_t faiss_hnsw_index_add(faiss_hnsw_t *index, int32_t n, const float *vectors);

/**
 * 在索引中搜索最近邻
 *
 * @param index      索引指针
 * @param query      查询向量，长度 dims
 * @param k          返回最近邻的数量
 * @param ef_search  搜索时的搜索宽度（越大越精确但越慢）
 * @param distances  输出距离数组，长度 k
 * @param ids        输出 id 数组，长度 k
 * @return           实际返回的结果数（成功等于 k），失败返回负数
 */
int32_t faiss_hnsw_index_search(faiss_hnsw_t *index, const float *query, int32_t k,
                                 int32_t ef_search, float *distances, int32_t *ids);

/**
 * 销毁索引并释放所有资源
 *
 * @param index 索引指针
 */
void faiss_hnsw_index_drop(faiss_hnsw_t *index);

#ifdef __cplusplus
}
#endif

#endif  // FAISS_HNSW_H