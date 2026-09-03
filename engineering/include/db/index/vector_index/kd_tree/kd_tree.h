/*
 * kd_tree.h — KD-Tree 索引 API 定义
 *
 * === 算法原理 ===
 * 1. 空间划分：递归选择切分维度和切分点
 * 2. kNN 搜索：优先队列 + 剪枝策略
 * 3. 最近邻：回溯检查兄弟节点
 *
 * === 参考实现 ===
 * KD-Tree: Bentley 1975
 */

#ifndef KD_TREE_H
#define KD_TREE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* KD-Tree 索引句柄 */
typedef struct kd_tree kd_tree_t;

/**
 * 创建 KD-Tree 索引
 *
 * @param dims 向量维度
 * @return 索引句柄，失败返回 NULL
 */
kd_tree_t *kd_tree_create(int dims);

/**
 * 销毁 KD-Tree 索引
 */
void kd_tree_destroy(kd_tree_t *idx);

/**
 * 插入向量到索引
 *
 * @param idx 索引句柄
 * @param id 向量 ID
 * @param vector 向量数据 [dims]
 * @return 0 成功，-1 失败
 */
int kd_tree_insert(kd_tree_t *idx, int id, const float *vector);

/**
 * 构建索引（批量构建）
 *
 * @param idx 索引句柄
 * @param n 向量数量
 * @param vectors 向量数据 [n * dims]
 * @param ids 向量 ID 数组 [n]
 * @return 0 成功，-1 失败
 */
int kd_tree_build(kd_tree_t *idx, int n, const float *vectors, const int *ids);

/**
 * 搜索最近邻
 *
 * @param idx 索引句柄
 * @param query 查询向量 [dims]
 * @param k 返回结果数量
 * @param result_ids 输出：结果 ID 数组 [k]
 * @param result_dists 输出：结果距离数组 [k]（可为 NULL）
 * @return 实际返回的结果数，-1 表示失败
 */
int kd_tree_search(const kd_tree_t *idx, const float *query, int k,
                   int *result_ids, float *result_dists);

/**
 * 搜索半径内的所有点
 *
 * @param idx 索引句柄
 * @param query 查询向量 [dims]
 * @param radius 搜索半径
 * @param result_ids 输出：结果 ID 数组
 * @param max_results 最大结果数
 * @return 实际返回的结果数，-1 表示失败
 */
int kd_tree_range_search(const kd_tree_t *idx, const float *query, float radius,
                         int *result_ids, int max_results);

/**
 * 保存索引到文件
 */
int kd_tree_save(const kd_tree_t *idx, const char *path);

/**
 * 从文件加载索引
 */
kd_tree_t *kd_tree_load(const char *path);

/**
 * 获取向量数量
 */
int kd_tree_get_n_items(const kd_tree_t *idx);

/**
 * 获取节点数量（包括内部节点）
 */
int kd_tree_get_n_nodes(const kd_tree_t *idx);

#ifdef __cplusplus
}
#endif

#endif /* KD_TREE_H */
