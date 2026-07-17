/*
 * itq.h
 *
 * ITQ (Iterative Quantization) 迭代量化
 *
 * 功能：
 * - 学习最优旋转变换
 * - 生成平衡的二进制码
 * - 高精度哈希编码
 *
 * 原理：
 * 1. PCA 降维
 * 2. 迭代优化旋转矩阵
 * 3. 二值化得到哈希码
 *
 * 使用示例：
 * @code
 *   itq_t *itq = itq_create(128, 32);
 *   // dims=128, n_bits=32
 *
 *   itq_train(itq, n_train, train_vectors);
 *
 *   uint32_t code = itq_encode(itq, vector);
 *
 *   itq_destroy(itq);
 * @endcode
 */

#ifndef DB_INDEX_VECTOR_ITQ_H
#define DB_INDEX_VECTOR_ITQ_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */
typedef struct itq itq_t;

/* ── 核心 API ── */

/**
 * 创建 ITQ 量化器
 * @param dims 向量维度
 * @param n_bits 哈希位数
 * @return 量化器指针，失败返回 NULL
 */
itq_t *itq_create(int dims, int n_bits);

/**
 * 训练量化器
 * @param itq 量化器指针
 * @param n 训练向量数量
 * @param vectors 训练向量 [n, dims]
 * @return 0 成功，-1 失败
 */
int itq_train(itq_t *itq, int n, const float *vectors);

/**
 * 编码向量
 * @param itq 量化器指针
 * @param vector 输入向量 [dims]
 * @return 哈希码
 */
uint32_t itq_encode(const itq_t *itq, const float *vector);

/**
 * 批量编码
 * @param itq 量化器指针
 * @param n 向量数量
 * @param vectors 向量数组 [n, dims]
 * @param codes 输出：哈希码数组
 * @return 0 成功，-1 失败
 */
int itq_encode_batch(const itq_t *itq, int n, const float *vectors, uint32_t *codes);

/**
 * 计算汉明距离
 * @param code1 哈希码 1
 * @param code2 哈希码 2
 * @return 汉明距离
 */
int itq_hamming_distance(uint32_t code1, uint32_t code2);

/**
 * 获取量化器信息
 * @param itq 量化器指针
 * @param out_dims 输出：维度
 * @param out_n_bits 输出：位数
 */
void itq_get_info(const itq_t *itq, int *out_dims, int *out_n_bits);

/**
 * 保存量化器
 * @param itq 量化器指针
 * @param path 文件路径
 * @return 0 成功，-1 失败
 */
int itq_save(const itq_t *itq, const char *path);

/**
 * 加载量化器
 * @param path 文件路径
 * @return 量化器指针，失败返回 NULL
 */
itq_t *itq_load(const char *path);

/**
 * 销毁量化器
 * @param itq 量化器指针
 */
void itq_destroy(itq_t *itq);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_ITQ_H */