/*
 * rq.h
 *
 * RQ (Residual Quantization) 残差量化器
 *
 * 功能：
 * - 多级残差量化
 * - 递归量化残差向量
 * - 高压缩比
 *
 * 原理：
 * 1. 第一级：对原始向量进行量化，得到量化中心和残差
 * 2. 第二级：对残差进行量化
 * 3. 重复直到达到指定级数
 *
 * 使用示例：
 * @code
 *   rq_t *rq = rq_create(128, 8, 4);
 *   // dims=128, n_centroids=256 (8 bits), n_stages=4
 *
 *   rq_train(rq, n_train, train_vectors);
 *
 *   uint8_t *codes = (uint8_t *)malloc(n * n_stages * sizeof(uint8_t));
 *   rq_encode(rq, n, vectors, codes);
 *
 *   float *decoded = (float *)malloc(n * dims * sizeof(float));
 *   rq_decode(rq, n, codes, decoded);
 *
 *   rq_destroy(rq);
 * @endcode
 */

#ifndef DB_INDEX_VECTOR_RQ_H
#define DB_INDEX_VECTOR_RQ_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */
typedef struct rq rq_t;

/* ── 核心 API ── */

/**
 * 创建 RQ 量化器
 * @param dims 向量维度
 * @param bits_per_stage 每级量化位数（决定每级中心数 = 2^bits）
 * @param n_stages 量化级数
 * @return 量化器指针，失败返回 NULL
 */
rq_t *rq_create(int dims, int bits_per_stage, int n_stages);

/**
 * 训练量化器（学习各级的码书）
 * @param rq 量化器指针
 * @param n 训练向量数量
 * @param vectors 训练向量数组 [n, dims]
 * @return 0 成功，-1 失败
 */
int rq_train(rq_t *rq, int n, const float *vectors);

/**
 * 编码向量
 * @param rq 量化器指针
 * @param n 向量数量
 * @param vectors 原始向量 [n, dims]
 * @param codes 输出：编码（每向量 n_stages 字节）
 * @return 0 成功，-1 失败
 */
int rq_encode(rq_t *rq, int n, const float *vectors, uint8_t *codes);

/**
 * 解码向量
 * @param rq 量化器指针
 * @param n 向量数量
 * @param codes 编码 [n, n_stages]
 * @param vectors 输出：解码向量 [n, dims]
 * @return 0 成功，-1 失败
 */
int rq_decode(rq_t *rq, int n, const uint8_t *codes, float *vectors);

/**
 * 计算量化距离（用于 ADC 搜索）
 * @param rq 量化器指针
 * @param query 查询向量 [dims]
 * @param codes 编码 [n_stages]
 * @return 近似距离
 */
float rq_compute_distance(const rq_t *rq, const float *query, const uint8_t *codes);

/**
 * 获取量化器信息
 * @param rq 量化器指针
 * @param out_dims 输出：向量维度
 * @param out_bits 输出：每级位数
 * @param out_stages 输出：级数
 * @param out_code_size 输出：码字大小（字节）
 */
void rq_get_info(const rq_t *rq, int *out_dims, int *out_bits,
                 int *out_stages, int *out_code_size);

/**
 * 保存量化器到文件
 * @param rq 量化器指针
 * @param path 文件路径
 * @return 0 成功，-1 失败
 */
int rq_save(const rq_t *rq, const char *path);

/**
 * 从文件加载量化器
 * @param path 文件路径
 * @return 量化器指针，失败返回 NULL
 */
rq_t *rq_load(const char *path);

/**
 * 销毁量化器
 * @param rq 量化器指针
 */
void rq_destroy(rq_t *rq);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_RQ_H */