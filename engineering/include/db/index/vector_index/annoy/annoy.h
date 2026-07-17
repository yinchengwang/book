/*
 * annoy.h — Annoy 索引 API 定义
 *
 * === 算法原理 ===
 * 1. 随机投影树：随机选择超平面递归划分空间
 * 2. 多树搜索：构建多棵树提高召回率
 * 3. 优先队列：维护 top-k 最近邻
 *
 * === 参考实现 ===
 * Spotify Annoy: https://github.com/spotify/annoy
 */

#ifndef ANNOY_H
#define ANNOY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Annoy 索引句柄 */
typedef struct annoy_index annoy_index_t;

/**
 * 创建 Annoy 索引
 *
 * @param dims 向量维度
 * @param metric 距离度量（"angular" 或 "euclidean"）
 * @return 索引句柄，失败返回 NULL
 */
annoy_index_t *annoy_create(int dims, const char *metric);

/**
 * 销毁 Annoy 索引
 */
void annoy_destroy(annoy_index_t *idx);

/**
 * 添加向量到索引
 *
 * @param idx 索引句柄
 * @param id 向量 ID
 * @param vector 向量数据 [dims]
 * @return 0 成功，-1 失败
 */
int annoy_add_item(annoy_index_t *idx, int id, const float *vector);

/**
 * 构建索引（构建随机投影树）
 *
 * @param idx 索引句柄
 * @param n_trees 树的数量（越多召回率越高，但构建时间越长）
 * @return 0 成功，-1 失败
 */
int annoy_build(annoy_index_t *idx, int n_trees);

/**
 * 搜索最近邻
 *
 * @param idx 索引句柄
 * @param query 查询向量 [dims]
 * @param k 返回结果数量
 * @param result_ids 输出：结果 ID 数组 [k]
 * @param result_dists 输出：结果距离数组 [k]（可为 NULL）
 * @param search_k 搜索节点数（-1 表示 n_trees * k）
 * @return 实际返回的结果数，-1 表示失败
 */
int annoy_search(const annoy_index_t *idx, const float *query, int k,
                 int *result_ids, float *result_dists, int search_k);

/**
 * 保存索引到文件
 */
int annoy_save(const annoy_index_t *idx, const char *path);

/**
 * 从文件加载索引
 */
annoy_index_t *annoy_load(const char *path);

/**
 * 获取向量数量
 */
int annoy_get_n_items(const annoy_index_t *idx);

/**
 * 获取向量数据（只读）
 */
const float *annoy_get_item(const annoy_index_t *idx, int id);

/**
 * 获取树的数量
 */
int annoy_get_n_trees(const annoy_index_t *idx);

#ifdef __cplusplus
}
#endif

#endif /* ANNOY_H */
