/*
 * opq.h
 *
 * OPQ (Optimized Product Quantization) 优化乘积量化
 *
 * 功能：
 * - 在 PQ 基础上增加 PCA 旋转优化子空间划分
 * - 使各子空间方差均衡，提升量化精度
 *
 * 使用示例：
 * @code
 *   opq_quantizer_t *opq = opq_create(128, 16, 8);  // dims, m, bits
 *
 *   opq_train(opq, n, train_vectors);
 *
 *   uint8_t code[16];
 *   opq_encode(opq, vector, code);
 *
 *   float table[4096];
 *   opq_compute_distance_table(opq, query, table);
 *   float dist = opq_adc_distance(opq, code, table);
 *
 *   opq_destroy(opq);
 * @endcode
 */

#ifndef DB_INDEX_VECTOR_OPQ_H
#define DB_INDEX_VECTOR_OPQ_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */
typedef struct storage_backend storage_backend_t;
typedef struct heap_vector_store heap_vector_store_t;
typedef struct opq_quantizer opq_quantizer_t;

/* ── OPQ 量化器结构体 ── */
struct opq_quantizer {
    int dims;           /* 向量维度 */
    int m;              /* 子空间数量 */
    int bits;           /* 每子空间比特数 */
    int ks;             /* 每子空间码本大小 = 2^bits */
    int sub_dim;        /* 子空间维度 = dims / m */
    float *codebooks;   /* 码本 [m, ks, sub_dim] */

    /* Phase 1 基础设施 */
    heap_vector_store_t *heap_store;  /* 存储量化编码 */
    storage_backend_t   *storage;     /* 存储后端 */

    int trained;        /* 是否已训练 */
};

/* ── 核心 API ── */

/**
 * 创建 OPQ 量化器
 * @param dims 向量维度
 * @param m 子空间数量
 * @param bits 每子空间比特数
 * @return OPQ 量化器指针，失败返回 NULL
 */
opq_quantizer_t *opq_create(int dims, int m, int bits);

/**
 * 训练 OPQ
 * @param opq OPQ 量化器
 * @param n 训练向量数量
 * @param vectors 训练向量数组 [n, dims]
 * @return 0 成功，-1 失败
 */
int opq_train(opq_quantizer_t *opq, int n, const float *vectors);

/**
 * 编码单个向量
 * @param opq OPQ 量化器
 * @param vector 输入向量 [dims]
 * @param code 输出编码 [m bytes]
 * @return 0 成功，-1 失败
 */
int opq_encode(opq_quantizer_t *opq, const float *vector, uint8_t *code);

/**
 * 批量编码
 * @param opq OPQ 量化器
 * @param n 向量数量
 * @param vectors 输入向量 [n, dims]
 * @param codes 输出编码 [n, m bytes]
 * @return 成功编码数量
 */
int opq_encode_batch(opq_quantizer_t *opq, int n, const float *vectors, uint8_t *codes);

/**
 * 解码向量
 * @param opq OPQ 量化器
 * @param code 输入编码 [m bytes]
 * @param vector 输出向量 [dims]
 * @return 0 成功，-1 失败
 */
int opq_decode(opq_quantizer_t *opq, const uint8_t *code, float *vector);

/**
 * 计算距离表
 * @param opq OPQ 量化器
 * @param query 查询向量 [dims]
 * @param table 输出距离表 [m, 2^bits]
 * @return 0 成功，-1 失败
 */
int opq_compute_distance_table(opq_quantizer_t *opq, const float *query, float *table);

/**
 * ADC 距离计算
 * @param opq OPQ 量化器
 * @param code 向量编码 [m bytes]
 * @param table 距离表
 * @return 近似距离
 */
float opq_adc_distance(opq_quantizer_t *opq, const uint8_t *code, const float *table);

/**
 * 获取编码大小
 * @param opq OPQ 量化器
 * @return 编码字节数
 */
int opq_code_size(const opq_quantizer_t *opq);

/**
 * 获取距离表大小
 * @param opq OPQ 量化器
 * @return 距离表元素数
 */
int opq_distance_table_size(const opq_quantizer_t *opq);

/**
 * 检查是否已训练
 * @param opq OPQ 量化器
 * @return 1 已训练，0 未训练
 */
int opq_is_trained(const opq_quantizer_t *opq);

/**
 * 销毁 OPQ 量化器
 * @param opq OPQ 量化器
 */
void opq_destroy(opq_quantizer_t *opq);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_OPQ_H */
