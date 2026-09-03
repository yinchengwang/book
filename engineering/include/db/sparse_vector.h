/**
 * @file sparse_vector.h
 * @brief 稀疏向量支持
 *
 * 定义稀疏向量结构及操作接口，用于 BM25 混合检索。
 * 稀疏向量仅存储非零元素，节省内存。
 */
#ifndef DB_SPARSE_VECTOR_H
#define DB_SPARSE_VECTOR_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 稀疏向量结构定义
 * ======================================================================== */

/**
 * @brief 稀疏向量
 *
 * 使用 COO (Coordinate) 格式存储：indices 存储非零维度索引，values 存储对应值。
 * indices 和 values 必须保持同步，按 indices 升序排列。
 */
typedef struct sparse_vector_s {
    uint32_t *indices;      /**< 非零维度索引数组 */
    float *values;          /**< 非零维度值数组 */
    uint32_t nnz;           /**< 当前非零元素数量 */
    uint32_t capacity;      /**< 已分配容量（indices/values 数组长度） */
    uint32_t dim;           /**< 总维度（含零元素） */
} sparse_vector_t;

/* ========================================================================
 * 稀疏向量 API
 * ======================================================================== */

/**
 * @brief 创建稀疏向量
 * @param dim 总维度
 * @return 成功返回稀疏向量指针，失败返回 NULL
 */
sparse_vector_t* sparse_vector_create(uint32_t dim);

/**
 * @brief 释放稀疏向量
 * @param vec 稀疏向量指针（可为 NULL）
 */
void sparse_vector_free(sparse_vector_t *vec);

/**
 * @brief 设置稀疏向量元素
 * @param vec 稀疏向量
 * @param index 维度索引（必须 < dim）
 * @param value 要设置的值（为 0 时等同于删除该元素）
 * @return 0 成功，-1 参数错误
 */
int sparse_vector_set(sparse_vector_t *vec, uint32_t index, float value);

/**
 * @brief 获取稀疏向量元素
 * @param vec 稀疏向量
 * @param index 维度索引
 * @return 该维度的值（不存在则返回 0）
 */
float sparse_vector_get(const sparse_vector_t *vec, uint32_t index);

/**
 * @brief 计算两个稀疏向量的点积
 * @param a 左操作数
 * @param b 右操作数
 * @return 点积结果（维度不匹配返回 0）
 */
float sparse_vector_dot_product(const sparse_vector_t *a, const sparse_vector_t *b);

/**
 * @brief 计算两个稀疏向量的余弦相似度
 * @param a 左操作数
 * @param b 右操作数
 * @return 余弦相似度 [-1, 1]，任一向量模为 0 返回 0
 */
float sparse_vector_cosine_similarity(const sparse_vector_t *a, const sparse_vector_t *b);

/**
 * @brief 从稠密向量转换为稀疏向量
 * @param dense 稠密向量数组
 * @param dim 维度
 * @param threshold 绝对值小于此阈值的元素视为零
 * @return 成功返回稀疏向量指针，失败返回 NULL
 */
sparse_vector_t* sparse_vector_from_dense(const float *dense, uint32_t dim, float threshold);

#ifdef __cplusplus
}
#endif

#endif /* DB_SPARSE_VECTOR_H */
