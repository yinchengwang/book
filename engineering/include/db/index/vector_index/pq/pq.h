/**
 * @file pq.h
 * @brief PQ (Product Quantization) 乘积量化
 *
 * 将高维向量分解为多个低维子向量，每个子向量独立量化。
 * 核心思想：
 * - 向量分解：D 维向量 → M 个 D/M 维子向量
 * - 独立量化：每个子空间用 K-Means 训练码本
 * - 压缩比：D × 4 字节 → M × 1 字节（bits=8 时）
 *
 * 距离计算方式：
 * - SDT（Symmetric Distance Transform）：查询点也量化
 * - ADC（Asymmetric Distance Compute）：查询点不量化，查表计算
 *
 * 使用示例：
 * @code
 *   pq_quantizer_t *pq = pq_create(128, 16, 8);  // dims, m, bits
 *
 *   pq_train(pq, n, train_vectors);
 *
 *   uint8_t code[16];
 *   pq_encode(pq, vector, code);
 *
 *   float table[4096];
 *   pq_compute_distance_table(pq, query, table);
 *   float dist = pq_adc_distance(pq, code, table);
 *
 *   pq_destroy(pq);
 * @endcode
 */

#ifndef DB_INDEX_VECTOR_PQ_H
#define DB_INDEX_VECTOR_PQ_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct storage_backend storage_backend_t;
typedef struct heap_vector_store heap_vector_store_t;

/**
 * @brief PQ 量化器结构体
 */
typedef struct {
    int dims;           /* 向量维度 */
    int m;              /* 子空间数量 */
    int bits;           /* 每子空间比特数 */
    int ks;             /* 每子空间码本大小 = 2^bits */
    int sub_dim;        /* 子空间维度 = dims / m */
    float *codebooks;   /* 码本 [m, ks, sub_dim] */

    /* 存储集成 */
    heap_vector_store_t *heap_store;
    storage_backend_t *storage;

    int trained;        /* 是否已训练 */
} pq_quantizer_t;

/* ========== 生命周期管理 ========== */

/**
 * @brief 创建 PQ 量化器
 * @param dims 向量维度
 * @param m 子空间数量
 * @param bits 每子空间比特数（通常为 8）
 * @return PQ 量化器指针，失败返回 NULL
 */
pq_quantizer_t *pq_create(int dims, int m, int bits);

/**
 * @brief 销毁 PQ 量化器
 * @param pq PQ 量化器
 */
void pq_destroy(pq_quantizer_t *pq);

/* ========== 训练与编码 ========== */

/**
 * @brief 训练 PQ 码本
 * @param pq PQ 量化器
 * @param n 训练向量数量
 * @param vectors 训练向量数组 [n, dims]
 * @return 0 成功，-1 失败
 */
int pq_train(pq_quantizer_t *pq, int n, const float *vectors);

/**
 * @brief 编码单个向量
 * @param pq PQ 量化器
 * @param vector 输入向量 [dims]
 * @param code 输出编码 [m bytes]
 * @return 0 成功，-1 失败
 */
int pq_encode(pq_quantizer_t *pq, const float *vector, uint8_t *code);

/**
 * @brief 批量编码向量
 * @param pq PQ 量化器
 * @param n 向量数量
 * @param vectors 输入向量 [n, dims]
 * @param codes 输出编码 [n, m bytes]
 * @return 成功编码数量
 */
int pq_encode_batch(pq_quantizer_t *pq, int n, const float *vectors, uint8_t *codes);

/**
 * @brief 解码单个编码
 * @param pq PQ 量化器
 * @param code 输入编码 [m bytes]
 * @param vector 输出向量 [dims]
 * @return 0 成功，-1 失败
 */
int pq_decode(pq_quantizer_t *pq, const uint8_t *code, float *vector);

/* ========== 距离计算 ========== */

/**
 * @brief 计算查询向量的距离表
 * @param pq PQ 量化器
 * @param query 查询向量 [dims]
 * @param table 输出距离表 [m * ks]
 * @return 0 成功，-1 失败
 *
 * 距离表存储每个子空间每个码字的距离：
 * table[sub * ks + c] = ||query_sub - codebook_sub[c]||^2
 */
int pq_compute_distance_table(pq_quantizer_t *pq, const float *query, float *table);

/**
 * @brief 使用距离表计算 ADC 距离
 * @param pq PQ 量化器
 * @param code 向量编码 [m bytes]
 * @param table 距离表
 * @return 近似距离
 */
float pq_adc_distance(pq_quantizer_t *pq, const uint8_t *code, const float *table);

/**
 * @brief 计算两个编码之间的距离
 * @param pq PQ 量化器
 * @param code1 编码1 [m bytes]
 * @param code2 编码2 [m bytes]
 * @return 距离
 */
float pq_sdt_distance(pq_quantizer_t *pq, const uint8_t *code1, const uint8_t *code2);

/* ========== 状态查询 ========== */

/**
 * @brief 获取编码大小
 * @return 编码字节数
 */
int pq_code_size(const pq_quantizer_t *pq);

/**
 * @brief 获取距离表大小
 * @return 距离表元素数
 */
int pq_distance_table_size(const pq_quantizer_t *pq);

/**
 * @brief 检查是否已训练
 * @return 1 已训练，0 未训练
 */
int pq_is_trained(const pq_quantizer_t *pq);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_PQ_H */