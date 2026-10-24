/*
 * spectral_hash.h
 *
 * Spectral Hash 频谱哈希
 *
 * 功能：
 * - 基于谱分析的哈希函数学习
 * - 生成平衡的二进制码
 * - 适合高维数据的近似搜索
 *
 * 原理：
 * 1. 计算数据矩阵的协方差
 * 2. 进行特征分解
 * 3. 使用前 k 个特征向量构造哈希函数
 *
 * 使用示例：
 * @code
 *   spectral_hash_t *sh = spectral_hash_create(128, 32);
 *   // dims=128, n_bits=32
 *
 *   spectral_hash_train(sh, n_train, train_vectors);
 *
 *   uint32_t code = spectral_hash_encode(sh, vector);
 *
 *   spectral_hash_destroy(sh);
 * @endcode
 */

#ifndef DB_INDEX_VECTOR_SPECTRAL_HASH_H
#define DB_INDEX_VECTOR_SPECTRAL_HASH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */
typedef struct spectral_hash spectral_hash_t;

/* ── 核心 API ── */

/**
 * 创建 Spectral Hash 量化器
 * @param dims 向量维度
 * @param n_bits 哈希位数
 * @return 量化器指针，失败返回 NULL
 */
spectral_hash_t *spectral_hash_create(int dims, int n_bits);

/**
 * 训练哈希函数
 * @param sh 量化器指针
 * @param n 训练向量数量
 * @param vectors 训练向量数组 [n, dims]
 * @return 0 成功，-1 失败
 */
int spectral_hash_train(spectral_hash_t *sh, int n, const float *vectors);

/**
 * 编码向量
 * @param sh 量化器指针
 * @param vector 输入向量 [dims]
 * @return 哈希码
 */
uint32_t spectral_hash_encode(const spectral_hash_t *sh, const float *vector);

/**
 * 批量编码
 * @param sh 量化器指针
 * @param n 向量数量
 * @param vectors 向量数组 [n, dims]
 * @param codes 输出：哈希码数组
 * @return 0 成功，-1 失败
 */
int spectral_hash_encode_batch(const spectral_hash_t *sh, int n,
                               const float *vectors, uint32_t *codes);

/**
 * 计算哈希距离（汉明距离）
 * @param code1 哈希码 1
 * @param code2 哈希码 2
 * @param n_bits 位数
 * @return 汉明距离
 */
int spectral_hash_hamming_distance(uint32_t code1, uint32_t code2, int n_bits);

/**
 * 获取量化器信息
 * @param sh 量化器指针
 * @param out_dims 输出：维度
 * @param out_n_bits 输出：位数
 */
void spectral_hash_get_info(const spectral_hash_t *sh, int *out_dims, int *out_n_bits);

/**
 * 保存量化器到文件
 * @param sh 量化器指针
 * @param path 文件路径
 * @return 0 成功，-1 失败
 */
int spectral_hash_save(const spectral_hash_t *sh, const char *path);

/**
 * 从文件加载量化器
 * @param path 文件路径
 * @return 量化器指针，失败返回 NULL
 */
spectral_hash_t *spectral_hash_load(const char *path);

/**
 * 销毁量化器
 * @param sh 量化器指针
 */
void spectral_hash_destroy(spectral_hash_t *sh);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_SPECTRAL_HASH_H */