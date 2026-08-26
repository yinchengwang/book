/**
 * @file bq.h
 * @brief BQ (Binary Quantization) 二值化量化
 *
 * 核心算法：
 * - 阈值选择：均值 / 中位数 / 学习阈值
 * - 位向量存储：每维 1 bit，32 维 = 4 字节
 * - Hamming 距离：bit 异或 + popcount
 * - 内存压缩率：32x（float32 → bit）
 *
 * 使用示例：
 * @code
 *   bq_quantizer_t *bq = bq_create(128, BQ_THRESHOLD_MEAN);
 *
 *   bq_train(bq, n, train_vectors);
 *
 *   uint8_t code[16];  // 128 / 8 = 16 bytes
 *   bq_encode(bq, vector, code);
 *
 *   // 查询时计算 Hamming 距离
 *   int dist = bq_hamming_distance(code1, code2);
 *
 *   bq_destroy(bq);
 * @endcode
 */

#ifndef DB_INDEX_VECTOR_BQ_H
#define DB_INDEX_VECTOR_BQ_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

#define BQ_MAX_DIMS 4096          /** 最大维度 */
#define BQ_CODE_SIZE(dims) (((dims) + 7) / 8)  /** 位向量字节数 */

/* ============================================================
 * 阈值选择策略
 * ============================================================ */

/**
 * @brief 阈值选择策略
 */
typedef enum BqThresholdStrategy {
    BQ_THRESHOLD_MEAN = 0,       /** 均值阈值 */
    BQ_THRESHOLD_MEDIAN = 1,     /** 中位数阈值 */
    BQ_THRESHOLD_ADAPTIVE = 2,   /** 自适应阈值（每维独立） */
    BQ_THRESHOLD_LEARNED = 3      /** 学习阈值（需训练） */
} BqThresholdStrategy_t;

/* ============================================================
 * 量化器结构
 * ============================================================ */

/**
 * @brief BQ 量化器结构体
 */
typedef struct {
    int dims;                       /** 向量维度 */
    BqThresholdStrategy_t strategy; /** 阈值策略 */

    /* 阈值参数 */
    float *thresholds;              /** 每维阈值 [dims] */
    float *means;                   /** 每维均值 [dims] */
    int trained;                     /** 是否已训练 */

    /* 统计信息 */
    int64_t n_samples;               /** 训练样本数 */
    float training_time_ms;          /** 训练耗时（毫秒） */
} bq_quantizer_t;

/**
 * @brief 学习阈值器（可选扩展）
 *
 * 使用 k-Means 找到最优二值划分
 */
typedef struct {
    int dims;                       /** 向量维度 */
    float *thresholds;              /** 学习到的阈值 [dims] */
    int trained;                    /** 是否已训练 */
} bq_learned_thresholds_t;

/* ============================================================
 * 生命周期管理
 * ============================================================ */

/**
 * @brief 创建 BQ 量化器
 * @param dims 向量维度
 * @param strategy 阈值策略
 * @return BQ 量化器指针，失败返回 NULL
 */
bq_quantizer_t *bq_create(int dims, BqThresholdStrategy_t strategy);

/**
 * @brief 销毁 BQ 量化器
 * @param bq BQ 量化器
 */
void bq_destroy(bq_quantizer_t *bq);

/* ============================================================
 * 训练与编码
 * ============================================================ */

/**
 * @brief 训练 BQ 阈值
 * @param bq BQ 量化器
 * @param n 训练向量数量
 * @param vectors 训练向量数组 [n, dims]
 * @return 0 成功，-1 失败
 */
int bq_train(bq_quantizer_t *bq, int n, const float *vectors);

/**
 * @brief 使用学习算法训练阈值（可选）
 *
 * 使用迭代方法找到每维的最优阈值
 * @param dims 维度
 * @param n 训练向量数量
 * @param vectors 训练向量
 * @param max_iter 最大迭代次数
 * @return 学习到的阈值器，失败返回 NULL
 */
bq_learned_thresholds_t *bq_learn_thresholds(int dims, int n,
                                             const float *vectors,
                                             int max_iter);

/**
 * @brief 销毁学习阈值器
 * @param lt 阈值器
 */
void bq_learned_thresholds_destroy(bq_learned_thresholds_t *lt);

/**
 * @brief 编码单个向量
 * @param bq BQ 量化器
 * @param vector 输入向量 [dims]
 * @param code 输出编码 [dims/8 bytes]
 * @return 0 成功，-1 失败
 *
 * 算法：code[bit] = (vector[bit] >= threshold[bit]) ? 1 : 0
 */
int bq_encode(const bq_quantizer_t *bq, const float *vector, uint8_t *code);

/**
 * @brief 批量编码向量
 * @param bq BQ 量化器
 * @param n 向量数量
 * @param vectors 输入向量 [n, dims]
 * @param codes 输出编码 [n, dims/8 bytes]
 * @return 成功编码数量，-1 失败
 */
int bq_encode_batch(const bq_quantizer_t *bq, int n,
                    const float *vectors, uint8_t *codes);

/**
 * @brief 解码单个编码
 * @param bq BQ 量化器
 * @param code 输入编码 [dims/8 bytes]
 * @param vector 输出向量 [dims]
 * @param value_if_0 解码为 0 时恢复的原始值（通常为阈值 - stddev）
 * @param value_if_1 解码为 1 时恢复的原始值（通常为阈值 + stddev）
 * @return 0 成功，-1 失败
 *
 * 注意：解码是近似的，只能恢复两个离散值
 */
int bq_decode(const bq_quantizer_t *bq, const uint8_t *code,
               float *vector, float value_if_0, float value_if_1);

/* ============================================================
 * 距离计算
 * ============================================================ */

/**
 * @brief 计算两个位向量的 Hamming 距离
 * @param code1 编码1 [dims/8 bytes]
 * @param code2 编码2 [dims/8 bytes]
 * @param code_size 编码字节数
 * @return Hamming 距离（0 到 code_size*8）
 *
 * Hamming 距离 = XOR 后 1 的位数 = popcount(code1 XOR code2)
 */
int bq_hamming_distance(const uint8_t *code1, const uint8_t *code2,
                         int code_size);

/**
 * @brief 计算位向量与批量编码的 Hamming 距离
 * @param code 查询编码
 * @param codes 数据库编码数组 [n, code_size]
 * @param n 编码数量
 * @param code_size 单个编码字节数
 * @param distances 输出距离数组 [n]（可为 NULL）
 * @return 最近的编码索引，-1 失败
 */
int bq_hamming_batch(const uint8_t *code, const uint8_t *codes,
                     int n, int code_size, int *distances);

/**
 * @brief 计算汉明距离表（用于 BinaryIVF）
 * @param code_size 编码字节数
 * @return 距离表大小（每个 byte 有 256 种组合）
 */
#define BQ_DISTANCE_TABLE_SIZE 256

/**
 * @brief 计算查询编码的距离表
 *
 * 预先计算每个可能字节值与查询的距离
 * 用于加速 BinaryIVF 的倒排列表遍历
 *
 * @param query_code 查询编码
 * @param code_size 编码字节数
 * @param table 输出距离表 [code_size * 256]，类型为 uint16_t 以支持大 code_size
 *
 * table[byte_idx * 256 + byte_val] = popcount(query_code[byte_idx] XOR byte_val)
 */
void bq_compute_distance_table(const uint8_t *query_code, int code_size,
                               uint16_t *table);

/**
 * @brief 使用预计算距离表计算总 Hamming 距离
 * @param table 距离表（uint16_t 类型）
 * @param code_size 编码字节数
 * @param target_code 目标编码
 * @return 总 Hamming 距离
 */
int bq_distance_from_table(const uint16_t *table, int code_size,
                           const uint8_t *target_code);

/* ============================================================
 * 状态查询
 * ============================================================ */

/**
 * @brief 获取编码大小
 * @param dims 维度
 * @return 编码字节数
 */
int bq_code_size(int dims);

/**
 * @brief 获取量化器编码大小
 * @param bq BQ 量化器
 * @return 编码字节数
 */
int bq_get_code_size(const bq_quantizer_t *bq);

/**
 * @brief 检查是否已训练
 * @param bq BQ 量化器
 * @return 1 已训练，0 未训练
 */
int bq_is_trained(const bq_quantizer_t *bq);

/**
 * @brief 获取内存压缩率
 * @param dims 向量维度
 * @return 压缩率（float32_bits / bit_bits）
 *
 * 例如 dims=128 时，压缩率为 128*4 / 128 = 4
 * 即每个向量从 512 字节压缩到 16 字节
 */
float bq_compression_ratio(int dims);

/**
 * @brief 获取阈值
 * @param bq BQ 量化器
 * @param dim 维度索引
 * @return 该维度的阈值
 */
float bq_get_threshold(const bq_quantizer_t *bq, int dim);

/**
 * @brief 打印量化器信息
 * @param bq BQ 量化器
 */
void bq_print_info(const bq_quantizer_t *bq);

/* ============================================================
 * 工具函数
 * ============================================================ */

/**
 * @brief popcount - 计算 64 位整数的 1 位个数
 *
 * 使用编译器内置函数，跨平台兼容
 */
static inline int bq_popcount64(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(x);
#elif defined(_MSC_VER)
    return __popcnt64(x);
#else
    /* 软件实现作为 fallback */
    x = x - ((x >> 1) & 0x5555555555555555ULL);
    x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
    x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FULL;
    x = x + (x >> 8);
    x = x + (x >> 16);
    x = x + (x >> 32);
    return (int)(x & 0x7F);
#endif
}

/**
 * @brief 计算 32 位整数的 popcount
 */
static inline int bq_popcount32(uint32_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcount(x);
#elif defined(_MSC_VER)
    return __popcnt(x);
#else
    x = x - ((x >> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x = (x + (x >> 4)) & 0x0F0F0F0F;
    x = x + (x >> 8);
    x = x + (x >> 16);
    return (int)(x & 0x3F);
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_BQ_H */
