/**
 * @file gpu_simd.h
 * @brief SIMD 优化接口
 *
 * 提供 CPU 端的 SIMD 优化（AVX-512/AVX2/NEON），
 * 用于向量量化、距离计算等操作的加速。
 */
#ifndef DB_GPU_SIMD_H
#define DB_GPU_SIMD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * SIMD 指令集支持检测
 * ======================================================================== */

/**
 * @brief SIMD 指令集类型
 */
typedef enum {
    SIMD_NONE = 0,         /**< 无 SIMD 支持 */
    SIMD_SSE = 1,          /**< SSE 4.2 */
    SIMD_AVX = 2,          /**< AVX */
    SIMD_AVX2 = 3,         /**< AVX2 */
    SIMD_AVX512 = 4,       /**< AVX-512 */
    SIMD_NEON = 5,         /**< ARM NEON */
} simd_type_t;

/**
 * @brief 获取当前平台支持的最高 SIMD 级别
 * @return SIMD 类型
 */
simd_type_t simd_get_available(void);

/**
 * @brief 检查是否支持指定 SIMD 指令集
 * @param type SIMD 类型
 * @return true 支持，false 不支持
 */
bool simd_is_supported(simd_type_t type);

/**
 * @brief 获取 SIMD 类型名称
 * @param type SIMD 类型
 * @return 类型名称字符串
 */
const char *simd_get_name(simd_type_t type);

/* ========================================================================
 * 向量距离计算
 * ======================================================================== */

/**
 * @brief 计算 L2 距离（欧氏距离）
 *
 * @param a 向量 A
 * @param b 向量 B
 * @param dim 向量维度
 * @return L2 距离
 */
float simd_l2_distance(const float *a, const float *b, int32_t dim);

/**
 * @brief 批量计算 L2 距离
 *
 * @param query 查询向量
 * @param database 数据库向量 [n][dim]
 * @param n 数据库向量数量
 * @param dim 向量维度
 * @param distances 输出距离数组 [n]
 */
void simd_l2_distance_batch(const float *query, const float *database,
                            int32_t n, int32_t dim, float *distances);

/**
 * @brief 计算内积（点积）
 *
 * @param a 向量 A
 * @param b 向量 B
 * @param dim 向量维度
 * @return 内积值
 */
float simd_inner_product(const float *a, const float *b, int32_t dim);

/**
 * @brief 批量计算内积
 *
 * @param query 查询向量
 * @param database 数据库向量 [n][dim]
 * @param n 数据库向量数量
 * @param dim 向量维度
 * @param products 输出内积数组 [n]
 */
void simd_inner_product_batch(const float *query, const float *database,
                              int32_t n, int32_t dim, float *products);

/**
 * @brief 计算余弦相似度
 *
 * @param a 向量 A（需预归一化）
 * @param b 向量 B（需预归一化）
 * @param dim 向量维度
 * @return 余弦相似度（1 - 距离，范围 [0,1]）
 */
float simd_cosine_similarity(const float *a, const float *b, int32_t dim);

/**
 * @brief 批量计算余弦相似度
 *
 * @param query 查询向量（需预归一化）
 * @param database 数据库向量 [n][dim]（需预归一化）
 * @param n 数据库向量数量
 * @param dim 向量维度
 * @param similarities 输出相似度数组 [n]
 */
void simd_cosine_similarity_batch(const float *query, const float *database,
                                  int32_t n, int32_t dim, float *similarities);

/* ========================================================================
 * 向量归一化
 * ======================================================================== */

/**
 * @brief L2 归一化向量
 *
 * @param v 输入/输出向量
 * @param dim 向量维度
 */
void simd_normalize_l2(float *v, int32_t dim);

/**
 * @brief 批量归一化向量
 *
 * @param vectors 输入/输出向量数组 [n][dim]
 * @param n 向量数量
 * @param dim 向量维度
 */
void simd_normalize_l2_batch(float *vectors, int32_t n, int32_t dim);

/* ========================================================================
 * 向量量化（Product Quantization）
 * ======================================================================== */

/**
 * @brief PQ 编码单个向量
 *
 * @param vectors 向量数据 [n][dim]
 * @param n 向量数量
 * @param dim 向量维度
 * @param m PQ 子空间数
 * @param nbits 每子空间位数
 * @param codebooks 码书 [m][ncentroids][dim/m]
 * @param codes 输出编码 [n][m]（每子空间 1 字节索引）
 */
void simd_pq_encode(const float *vectors, int32_t n, int32_t dim,
                    int32_t m, int32_t nbits,
                    const float *codebooks, uint8_t *codes);

/**
 * @brief PQ 解码（重建向量）
 *
 * @param codes 编码数据 [n][m]
 * @param n 向量数量
 * @param m PQ 子空间数
 * @param nbits 每子空间位数
 * @param codebooks 码书
 * @param dim 向量维度
 * @param vectors 输出重建向量 [n][dim]
 */
void simd_pq_decode(const uint8_t *codes, int32_t n, int32_t m, int32_t nbits,
                    const float *codebooks, int32_t dim, float *vectors);

/**
 * @brief PQ 距离计算（非对称：查询与编码的距离）
 *
 * @param query 查询向量 [dim]
 * @param codes 编码数据 [n][m]
 * @param n 编码数量
 * @param m PQ 子空间数
 * @param nbits 每子空间位数
 * @param codebooks 码书
 * @param dim 向量维度
 * @param distances 输出距离数组 [n]
 */
void simd_pq_distance_async(const float *query, const uint8_t *codes,
                            int32_t n, int32_t m, int32_t nbits,
                            const float *codebooks, int32_t dim,
                            float *distances);

/**
 * @brief PQ 距离计算（对称：编码与编码的距离）
 *
 * @param codes1 编码数据 1 [n1][m]
 * @param codes2 编码数据 2 [n2][m]
 * @param n1 编码数量 1
 * @param n2 编码数量 2
 * @param m PQ 子空间数
 * @param nbits 每子空间位数
 * @param codebooks 码书
 * @param distances 输出距离数组 [n1][n2]
 */
void simd_pq_distance_sym(const uint8_t *codes1, const uint8_t *codes2,
                          int32_t n1, int32_t n2, int32_t m, int32_t nbits,
                          const float *codebooks, float *distances);

/* ========================================================================
 * 向量加法与减法
 * ======================================================================== */

/**
 * @brief 向量加法：c = a + b
 *
 * @param a 向量 A
 * @param b 向量 B
 * @param c 输出向量 C
 * @param dim 向量维度
 */
void simd_vec_add(const float *a, const float *b, float *c, int32_t dim);

/**
 * @brief 向量减法：c = a - b
 *
 * @param a 向量 A
 * @param b 向量 B
 * @param c 输出向量 C
 * @param dim 向量维度
 */
void simd_vec_sub(const float *a, const float *b, float *c, int32_t dim);

/**
 * @brief 向量乘加：a = a + k * b
 *
 * @param a 输入/输出向量 A
 * @param b 向量 B
 * @param k 标量系数
 * @param dim 向量维度
 */
void simd_vec_fma(const float *b, float *a, float k, int32_t dim);

/**
 * @brief 批量向量加法
 *
 * @param a 向量 A
 * @param b 向量 B [n][dim]
 * @param c 输出向量 C [n][dim]
 * @param n 向量数量
 * @param dim 向量维度
 */
void simd_vec_add_batch(const float *a, const float *b, float *c,
                        int32_t n, int32_t dim);

/* ========================================================================
 * 归约操作
 * ======================================================================== */

/**
 * @brief 求和归约
 *
 * @param v 向量
 * @param dim 向量维度
 * @return 求和结果
 */
float simd_reduce_sum(const float *v, int32_t dim);

/**
 * @brief 最大值归约
 *
 * @param v 向量
 * @param dim 向量维度
 * @return 最大值
 */
float simd_reduce_max(const float *v, int32_t dim);

/**
 * @brief 最小值归约
 *
 * @param v 向量
 * @param dim 向量维度
 * @return 最小值
 */
float simd_reduce_min(const float *v, int32_t dim);

/**
 * @brief 平方和归约
 *
 * @param v 向量
 * @param dim 向量维度
 * @return 平方和
 */
float simd_reduce_sqsum(const float *v, int32_t dim);

/* ========================================================================
 * Top-K 查找
 * ======================================================================== */

/**
 * @brief 查找 Top-K 最大值及其索引
 *
 * @param values 值数组 [n]
 * @param n 数组长度
 * @param k 要查找的数量
 * @param top_values 输出 Top-K 值
 * @param top_indices 输出 Top-K 索引
 */
void simd_topk(const float *values, int32_t n, int32_t k,
               float *top_values, int32_t *top_indices);

/**
 * @brief 查找 Top-K 最小值及其索引
 *
 * @param values 值数组 [n]
 * @param n 数组长度
 * @param k 要查找的数量
 * @param top_values 输出 Top-K 值
 * @param top_indices 输出 Top-K 索引
 */
void simd_topk_min(const float *values, int32_t n, int32_t k,
                   float *top_values, int32_t *top_indices);

/* ========================================================================
 * 性能提示
 * ======================================================================== */

/**
 * @brief 内存预取提示
 *
 * @param ptr 数据指针
 * @param hint 预取提示（0=读，1=写）
 */
void simd_prefetch(const void *ptr, int hint);

/**
 * @brief 编译时 SIMD 优化级别
 */
typedef enum {
    SIMD_OPT_NONE = 0,     /**< 无优化 */
    SIMD_OPT_AUTO = 1,     /**< 自动选择最佳 */
    SIMD_OPT_SSE = 2,      /**< 强制 SSE */
    SIMD_OPT_AVX2 = 3,     /**< 强制 AVX2 */
    SIMD_OPT_AVX512 = 4,   /**< 强制 AVX-512 */
} simd_opt_level_t;

/**
 * @brief 设置 SIMD 优化级别
 * @param level 优化级别
 */
void simd_set_opt_level(simd_opt_level_t level);

/**
 * @brief 获取当前 SIMD 优化级别
 * @return 当前优化级别
 */
simd_opt_level_t simd_get_opt_level(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_GPU_SIMD_H */
