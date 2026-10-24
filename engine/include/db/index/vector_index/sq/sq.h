/**
 * @file sq.h
 * @brief SQ (Scalar Quantization) 标量量化
 *
 * 将每个维度的浮点数量化为固定范围的整数值。
 * 核心思想：
 * - 确定每维的最小/最大值
 * - 线性映射到 [0, 255]（8-bit）
 * - 压缩比：D × 4 字节 → D × 1 字节
 *
 * 优点：
 * - 实现简单，速度快
 * - 内存占用小
 * - 适合低精度场景
 *
 * 缺点：
 * - 精度较低（无法利用维度间相关性）
 * - 仅适合均匀分布数据
 */

#ifndef DB_INDEX_VECTOR_SQ_H
#define DB_INDEX_VECTOR_SQ_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SQ 量化器结构体
 */
typedef struct {
    int dims;           /* 向量维度 */
    int bits;           /* 每维度比特数（通常为 8） */
    float *mins;        /* 每维最小值 [dims] */
    float *maxs;        /* 每维最大值 [dims] */
    float *scales;      /* 每维缩放因子 = 255 / (max - min) */
    int trained;        /* 是否已训练 */
} sq_quantizer_t;

/* ========== 生命周期管理 ========== */

/**
 * @brief 创建 SQ 量化器
 * @param dims 向量维度
 * @param bits 每维度比特数（通常为 8）
 * @return SQ 量化器指针，失败返回 NULL
 */
sq_quantizer_t *sq_create(int dims, int bits);

/**
 * @brief 销毁 SQ 量化器
 * @param sq SQ 量化器
 */
void sq_destroy(sq_quantizer_t *sq);

/* ========== 训练与编码 ========== */

/**
 * @brief 训练 SQ（确定每维范围）
 * @param sq SQ 量化器
 * @param n 训练向量数量
 * @param vectors 训练向量数组 [n, dims]
 * @return 0 成功，-1 失败
 */
int sq_train(sq_quantizer_t *sq, int n, const float *vectors);

/**
 * @brief 编码单个向量
 * @param sq SQ 量化器
 * @param vector 输入向量 [dims]
 * @param code 输出编码 [dims bytes]
 * @return 0 成功，-1 失败
 */
int sq_encode(sq_quantizer_t *sq, const float *vector, uint8_t *code);

/**
 * @brief 批量编码向量
 * @param sq SQ 量化器
 * @param n 向量数量
 * @param vectors 输入向量 [n, dims]
 * @param codes 输出编码 [n, dims bytes]
 * @return 成功编码数量
 */
int sq_encode_batch(sq_quantizer_t *sq, int n, const float *vectors, uint8_t *codes);

/**
 * @brief 解码单个编码
 * @param sq SQ 量化器
 * @param code 输入编码 [dims bytes]
 * @param vector 输出向量 [dims]
 * @return 0 成功，-1 失败
 */
int sq_decode(sq_quantizer_t *sq, const uint8_t *code, float *vector);

/* ========== 距离计算 ========== */

/**
 * @brief 计算编码与浮点向量的距离
 * @param sq SQ 量化器
 * @param code 编码向量 [dims bytes]
 * @param vector 浮点向量 [dims]
 * @return L2 距离平方
 */
float sq_distance_to_vector(sq_quantizer_t *sq, const uint8_t *code, const float *vector);

/**
 * @brief 计算两个编码之间的距离
 * @param sq SQ 量化器
 * @param code1 编码1 [dims bytes]
 * @param code2 编码2 [dims bytes]
 * @return L2 距离平方
 */
float sq_distance(sq_quantizer_t *sq, const uint8_t *code1, const uint8_t *code2);

/* ========== 状态查询 ========== */

/**
 * @brief 获取编码大小
 * @return 编码字节数
 */
int sq_code_size(const sq_quantizer_t *sq);

/**
 * @brief 检查是否已训练
 * @return 1 已训练，0 未训练
 */
int sq_is_trained(const sq_quantizer_t *sq);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_SQ_H */