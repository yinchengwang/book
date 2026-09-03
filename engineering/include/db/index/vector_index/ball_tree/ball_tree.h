/*
 * ball_tree.h
 *
 * Ball-Tree 索引
 *
 * 功能：
 * - 基于超球划分的空间索引
 * - kNN 搜索和范围搜索
 * - 比 KD-Tree 更适合高维数据
 *
 * 原理：
 * - 递归地将数据点划分到超球内
 * - 每个节点用一个中心点 + 半径定义
 * - 搜索时利用三角不等式剪枝
 *
 * 使用示例：
 * @code
 *   ball_tree_t *idx = ball_tree_create(16);
 *
 *   ball_tree_build(idx, n, vectors, ids);
 *
 *   int result_ids[10];
 *   float result_dists[10];
 *   ball_tree_search(idx, query, 10, result_ids, result_dists);
 *
 *   ball_tree_destroy(idx);
 * @endcode
 */

#ifndef DB_INDEX_VECTOR_BALL_TREE_H
#define DB_INDEX_VECTOR_BALL_TREE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */
typedef struct ball_tree ball_tree_t;

/* ── 核心 API ── */

/**
 * 创建 Ball-Tree 索引
 * @param dims 向量维度
 * @return 索引指针，失败返回 NULL
 */
ball_tree_t *ball_tree_create(int dims);

/**
 * 构建索引
 * @param idx 索引指针
 * @param n 向量数量
 * @param vectors 向量数组 [n, dims]
 * @param ids 向量 ID 数组
 * @return 0 成功，-1 失败
 */
int ball_tree_build(ball_tree_t *idx, int n, const float *vectors, const int *ids);

/**
 * 搜索 K 近邻
 * @param idx 索引指针
 * @param query 查询向量 [dims]
 * @param k 返回结果数量
 * @param ids 输出：匹配的向量 ID（需预分配 k 个）
 * @param distances 输出：对应的距离（需预分配 k 个）
 * @return 实际返回结果数量
 */
int ball_tree_search(const ball_tree_t *idx, const float *query,
                     int k, int *ids, float *distances);

/**
 * 范围搜索
 * @param idx 索引指针
 * @param query 查询向量 [dims]
 * @param radius 搜索半径
 * @param result_ids 输出：匹配的向量 ID（需预分配 max_results 个）
 * @param max_results 最大结果数
 * @return 实际返回结果数量
 */
int ball_tree_range_search(const ball_tree_t *idx, const float *query,
                           float radius, int *result_ids, int max_results);

/**
 * 获取统计信息
 * @param idx 索引指针
 * @param out_n_vectors 输出：向量数量
 * @param out_n_nodes 输出：节点数量
 */
void ball_tree_stats(const ball_tree_t *idx, int *out_n_vectors, int *out_n_nodes);

/**
 * 保存索引到文件
 * @param idx 索引指针
 * @param path 文件路径
 * @return 0 成功，-1 失败
 */
int ball_tree_save(const ball_tree_t *idx, const char *path);

/**
 * 从文件加载索引
 * @param path 文件路径
 * @return 索引指针，失败返回 NULL
 */
ball_tree_t *ball_tree_load(const char *path);

/**
 * 销毁索引
 * @param idx 索引指针
 */
void ball_tree_destroy(ball_tree_t *idx);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_BALL_TREE_H */