#ifndef HNSW_H
#define HNSW_H

#include <stdint.h>
#include <stdbool.h>
#include <algo-prod/distance/distance.h>
#include <algo-prod/quantization/quantization.h>

#ifdef __cplusplus
extern "C" {
#endif

// 持久化版 HNSW 索引（基于 pgVector 页面管理风格）
typedef struct hnsw_index hnsw_index_t;

// 创建新索引
hnsw_index_t *hnsw_index_create(const char *path, int32_t M, int32_t dims,
                                  int32_t ef_construction, distance_metric_t metric,
                                  quantization_type_t quant_type);

// 打开已有索引
hnsw_index_t *hnsw_index_open(const char *path);

// 关闭索引（写回元数据）
void hnsw_index_close(hnsw_index_t *idx);

// 插入向量
int32_t hnsw_index_add(hnsw_index_t *idx, int32_t n, const float *vectors);

// 搜索
int32_t hnsw_index_search(hnsw_index_t *idx, const float *query, int32_t k,
                           int32_t ef_search, float *distances, int32_t *ids);

// 删除索引
void hnsw_index_drop(hnsw_index_t *idx);

#ifdef __cplusplus
}
#endif
#endif
