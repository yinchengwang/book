/*
 * lvq.h
 *
 * LVQ (Learning Vector Quantization) 学习向量量化器
 *
 * 功能：
 * - 有监督的向量量化
 * - 学习区分不同类别的码书
 * - 适合分类任务的向量编码
 *
 * 原理：
 * - 初始化一组带标签的原型向量
 * - 对于每个训练样本，找到最近的原型
 * - 如果标签相同，拉近；标签不同，推远
 *
 * 使用示例：
 * @code
 *   lvq_t *lvq = lvq_create(128, 10);
 *   // dims=128, n_prototypes=256
 *
 *   lvq_train(lvq, n_train, train_vectors, labels);
 *
 *   int code = lvq_encode(lvq, vector);
 *
 *   lvq_destroy(lvq);
 * @endcode
 */

#ifndef DB_INDEX_VECTOR_LVQ_H
#define DB_INDEX_VECTOR_LVQ_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */
typedef struct lvq lvq_t;

/* ── 核心 API ── */

/**
 * 创建 LVQ 量化器
 * @param dims 向量维度
 * @param n_prototypes 原型数量
 * @return 量化器指针，失败返回 NULL
 */
lvq_t *lvq_create(int dims, int n_prototypes);

/**
 * 训练量化器
 * @param lvq 量化器指针
 * @param n 训练向量数量
 * @param vectors 训练向量 [n, dims]
 * @param labels 标签 [n]（整数标签）
 * @return 0 成功，-1 失败
 */
int lvq_train(lvq_t *lvq, int n, const float *vectors, const int *labels);

/**
 * 编码向量（找到最近的原型）
 * @param lvq 量化器指针
 * @param vector 输入向量 [dims]
 * @return 原型索引
 */
int lvq_encode(const lvq_t *lvq, const float *vector);

/**
 * 批量编码
 * @param lvq 量化器指针
 * @param n 向量数量
 * @param vectors 向量数组 [n, dims]
 * @param codes 输出：编码（原型索引）[n]
 * @return 0 成功，-1 失败
 */
int lvq_encode_batch(const lvq_t *lvq, int n, const float *vectors, int *codes);

/**
 * 获取原型向量
 * @param lvq 量化器指针
 * @param idx 原型索引
 * @param out_vector 输出向量（需预分配 dims 个 float）
 */
void lvq_get_prototype(const lvq_t *lvq, int idx, float *out_vector);

/**
 * 获取原型标签
 * @param lvq 量化器指针
 * @param idx 原型索引
 * @return 原型的标签
 */
int lvq_get_label(const lvq_t *lvq, int idx);

/**
 * 获取量化器信息
 * @param lvq 量化器指针
 * @param out_dims 输出：维度
 * @param out_n_prototypes 输出：原型数量
 * @param out_code_size 输出：码字大小（字节）
 */
void lvq_get_info(const lvq_t *lvq, int *out_dims, int *out_n_prototypes, int *out_code_size);

/**
 * 保存量化器到文件
 * @param lvq 量化器指针
 * @param path 文件路径
 * @return 0 成功，-1 失败
 */
int lvq_save(const lvq_t *lvq, const char *path);

/**
 * 从文件加载量化器
 * @param path 文件路径
 * @return 量化器指针，失败返回 NULL
 */
lvq_t *lvq_load(const char *path);

/**
 * 销毁量化器
 * @param lvq 量化器指针
 */
void lvq_destroy(lvq_t *lvq);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_LVQ_H */