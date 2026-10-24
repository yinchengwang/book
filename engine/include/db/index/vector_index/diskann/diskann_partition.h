/*
 * diskann_partition.h
 *
 * DiskANN 分区策略头文件。
 * 支持 Random、K-Means、Coordinate-Range 三种分区策略。
 */

#ifndef DISKANN_PARTITION_H
#define DISKANN_PARTITION_H

#include <stdbool.h>
#include <stdint.h>

#include <db/index/vector_index/diskann/diskann.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 分区元数据
 * ============================================================================ */

/**
 * @brief 分区元数据结构
 */
typedef struct diskann_partition {
    int32_t id;                      /**< 分区 ID */
    int32_t start;                   /**< 向量起始索引 */
    int32_t count;                   /**< 向量数量 */
    float *center;                   /**< 分区中心点坐标 (dims,) */
    float radius;                    /**< 分区半径 */
} diskann_partition_t;

/**
 * @brief 分区结果结构
 */
typedef struct diskann_partition_result {
    diskann_partition_t *partitions; /**< 分区数组 */
    int32_t partition_count;          /**< 分区数量 */
    int32_t *partition_ids;           /**< 每个向量所属分区 ID (n,) */
} diskann_partition_result_t;

/* ============================================================================
 * 分区策略函数
 * ============================================================================ */

/**
 * @brief 验证分区配置
 * @param[in] method 分区策略
 * @param[in] partition_count 分区数量
 * @param[in] overlap_ratio 重叠比例
 * @return 有效返回 1，无效返回 0
 */
int32_t diskann_partition_validate_params(diskann_partition_method_t method,
                                         int32_t partition_count,
                                         float overlap_ratio);

/**
 * @brief 执行随机分区
 * @param[in] vectors 原始向量 (n * dims)
 * @param[in] n 向量数量
 * @param[in] dims 向量维度
 * @param[in] partition_count 分区数量
 * @param[out] partition_ids 每个向量所属分区 ID (n,)
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_partition_random(const float *vectors,
                                 int32_t n,
                                 int32_t dims,
                                 int32_t partition_count,
                                 int32_t *partition_ids);

/**
 * @brief 执行 K-Means 分区
 * @param[in] vectors 原始向量 (n * dims)
 * @param[in] n 向量数量
 * @param[in] dims 向量维度
 * @param[in] partition_count 分区数量
 * @param[out] partition_ids 每个向量所属分区 ID (n,)
 * @param[out] centers 分区中心点 (partition_count * dims)，可为 NULL
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_partition_kmeans(const float *vectors,
                                 int32_t n,
                                 int32_t dims,
                                 int32_t partition_count,
                                 int32_t *partition_ids,
                                 float *centers);

/**
 * @brief 执行坐标范围分区
 * @param[in] vectors 原始向量 (n * dims)
 * @param[in] n 向量数量
 * @param[in] dims 向量维度
 * @param[in] partition_count 分区数量
 * @param[out] partition_ids 每个向量所属分区 ID (n,)
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_partition_coordinate_range(const float *vectors,
                                           int32_t n,
                                           int32_t dims,
                                           int32_t partition_count,
                                           int32_t *partition_ids);

/**
 * @brief 执行分区（根据策略自动选择）
 * @param[in] vectors 原始向量 (n * dims)
 * @param[in] n 向量数量
 * @param[in] dims 向量维度
 * @param[in] method 分区策略
 * @param[in] partition_count 分区数量
 * @param[out] partition_ids 每个向量所属分区 ID (n,)
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_partition_data(const float *vectors,
                               int32_t n,
                               int32_t dims,
                               diskann_partition_method_t method,
                               int32_t partition_count,
                               int32_t *partition_ids);

/**
 * @brief 创建分区结果
 * @param[in] vectors 原始向量 (n * dims)
 * @param[in] n 向量数量
 * @param[in] dims 向量维度
 * @param[in] partition_ids 每个向量所属分区 ID (n,)
 * @param[in] partition_count 分区数量
 * @return 分区结果，失败返回 NULL
 */
diskann_partition_result_t *diskann_partition_result_create(const float *vectors,
                                                            int32_t n,
                                                            int32_t dims,
                                                            const int32_t *partition_ids,
                                                            int32_t partition_count);

/**
 * @brief 释放分区结果
 * @param[in] result 分区结果
 */
void diskann_partition_result_free(diskann_partition_result_t *result);

/**
 * @brief 计算向量到分区中心的距离
 * @param[in] vector 向量 (dims,)
 * @param[in] center 中心点 (dims,)
 * @param[in] dims 向量维度
 * @param[in] metric 距离度量
 * @return 距离值
 */
float diskann_partition_distance_to_center(const float *vector,
                                            const float *center,
                                            int32_t dims,
                                            distance_metric_t metric);

/**
 * @brief 获取重叠区域向量
 * @param[in] vectors 原始向量 (n * dims)
 * @param[in] n 向量数量
 * @param[in] dims 向量维度
 * @param[in] partition_ids 每个向量所属分区 ID (n,)
 * @param[in] partition_count 分区数量
 * @param[in] overlap_ratio 重叠比例
 * @param[out] overlap_count 重叠向量数量
 * @return 重叠向量 ID 数组，失败返回 NULL
 */
int32_t *diskann_partition_get_overlap_vectors(const float *vectors,
                                                int32_t n,
                                                int32_t dims,
                                                const int32_t *partition_ids,
                                                int32_t partition_count,
                                                float overlap_ratio,
                                                int32_t *overlap_count);

#ifdef __cplusplus
}
#endif

#endif /* DISKANN_PARTITION_H */
