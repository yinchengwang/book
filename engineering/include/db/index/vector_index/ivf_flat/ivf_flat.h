/*
 * ivf_flat.h — IVF-Flat (Inverted File Flat) 索引
 *
 * === 概述 ===
 * IVF-Flat 是最简单的 IVF 变体：
 *   1. 训练：K-Means 聚类产生 nlist 个中心
 *   2. 添加：每个向量路由到最近中心，存入对应倒排列表
 *   3. 搜索：查询向量找最近的 nprobe 个簇，扫描其中所有向量
 *
 * === 与 IVF-PQ 的区别 ===
 * IVF-Flat 不使用量化，直接存储原始向量：
 *   - 优点：精度高（无量化误差），实现简单
 *   - 缺点：内存占用大（存储原始向量），不适合超大规模
 *
 * === 适用场景 ===
 *   - 向量数 < 100 万
 *   - 内存充足
 *   - 需要高召回率的场景
 */

#ifndef IVF_FLAT_H
#define IVF_FLAT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IVF-Flat 索引结构（不透明句柄） */
typedef struct ivf_flat_index ivf_flat_index_t;

/**
 * 创建 IVF-Flat 索引
 *
 * @param nlist  聚类中心数量（通常为 sqrt(n_vectors) 到 n_vectors/100）
 * @param dims   向量维度
 * @return 索引句柄，失败返回 NULL
 */
ivf_flat_index_t *ivf_flat_create(int nlist, int dims);

/**
 * 销毁索引
 *
 * @param idx 索引句柄
 */
void ivf_flat_destroy(ivf_flat_index_t *idx);

/**
 * 训练索引（K-Means 聚类）
 *
 * @param idx      索引句柄
 * @param n        训练向量数量（建议 >= 100 * nlist）
 * @param vectors  训练向量数组 [n * dims]
 * @return 0 成功，-1 失败
 */
int ivf_flat_train(ivf_flat_index_t *idx, int n, const float *vectors);

/**
 * 添加向量到索引
 *
 * @param idx      索引句柄（必须已训练）
 * @param n        向量数量
 * @param vectors  向量数组 [n * dims]
 * @param ids      向量 ID 数组 [n]
 * @return 成功添加的向量数，失败返回 -1
 */
int ivf_flat_add(ivf_flat_index_t *idx, int n, const float *vectors, const int *ids);

/**
 * 搜索最近邻
 *
 * @param idx        索引句柄
 * @param query      查询向量 [dims]
 * @param k          返回结果数
 * @param result_ids 输出：结果 ID 数组 [k]
 * @param result_dists 输出：结果距离数组 [k]
 * @return 实际返回的结果数
 */
int ivf_flat_search(const ivf_flat_index_t *idx, const float *query, int k,
                    int *result_ids, float *result_dists);

/**
 * 设置搜索探针数
 *
 * @param idx     索引句柄
 * @param nprobe  探测簇数量（1 到 nlist）
 */
void ivf_flat_set_nprobe(ivf_flat_index_t *idx, int nprobe);

/**
 * 获取统计信息
 *
 * @param idx        索引句柄
 * @param n_vectors  输出：已添加向量数
 * @param n_clusters 输出：聚类中心数
 */
void ivf_flat_stats(const ivf_flat_index_t *idx, int *n_vectors, int *n_clusters);

/**
 * 持久化索引到文件
 *
 * @param idx   索引句柄
 * @param path  文件路径
 * @return 0 成功，-1 失败
 */
int ivf_flat_save(const ivf_flat_index_t *idx, const char *path);

/**
 * 从文件加载索引
 *
 * @param path 文件路径
 * @return 索引句柄，失败返回 NULL
 */
ivf_flat_index_t *ivf_flat_load(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* IVF_FLAT_H */