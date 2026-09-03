/*
 * diskann_spann.h
 *
 * DiskANN SPANN (SParse ANN) 分区层次索引头文件。
 * 支持分区索引以减少搜索时的 IO 操作。
 */

#ifndef DISKANN_SPANN_H
#define DISKANN_SPANN_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <db/index/vector_index/diskann/diskann.h>
#include <db/index/vector_index/diskann/diskann_partition.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * SPANN 上下文
 * ============================================================================ */

/**
 * @brief SPANN 上下文结构
 */
typedef struct diskann_spann_context {
    diskann_spann_config_t config;   /**< SPANN 配置 */
    diskann_partition_result_t *partition_result; /**< 分区结果 */

    /* 搜索相关 */
    float *query_distances;          /**< 查询到各分区中心的距离缓存 */
} diskann_spann_context_t;

/* ============================================================================
 * SPANN 函数
 * ============================================================================ */

/**
 * @brief 创建 SPANN 上下文
 * @param[in] config SPANN 配置
 * @return 上下文指针，失败返回 NULL
 */
diskann_spann_context_t *diskann_spann_create(const diskann_spann_config_t *config);

/**
 * @brief 释放 SPANN 上下文
 * @param[in] ctx SPANN 上下文
 */
void diskann_spann_free(diskann_spann_context_t *ctx);

/**
 * @brief 初始化 SPANN
 * @param[in] ctx SPANN 上下文
 * @param[in] vectors 原始向量 (n * dims)
 * @param[in] n 向量数量
 * @param[in] dims 向量维度
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_spann_init(diskann_spann_context_t *ctx,
                           const float *vectors,
                           int32_t n,
                           int32_t dims);

/**
 * @brief 构建分区
 * @param[in] ctx SPANN 上下文
 * @param[in] vectors 原始向量 (n * dims)
 * @param[in] n 向量数量
 * @param[in] dims 向量维度
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_spann_build_partitions(diskann_spann_context_t *ctx,
                                       const float *vectors,
                                       int32_t n,
                                       int32_t dims);

/**
 * @brief 计算分区中心
 * @param[in] ctx SPANN 上下文
 * @param[in] vectors 原始向量 (n * dims)
 * @param[in] n 向量数量
 * @param[in] dims 向量维度
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_spann_compute_centers(diskann_spann_context_t *ctx,
                                      const float *vectors,
                                      int32_t n,
                                      int32_t dims);

/**
 * @brief 选择要搜索的分区（路由）
 * @param[in] ctx SPANN 上下文
 * @param[in] query 查询向量 (dims,)
 * @param[in] dims 向量维度
 * @param[out] selected_partitions 选中的分区 ID 数组
 * @param[out] selected_count 选中的分区数量
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_spann_select_partitions(diskann_spann_context_t *ctx,
                                        const float *query,
                                        int32_t dims,
                                        int32_t **selected_partitions,
                                        int32_t *selected_count);

/**
 * @brief 在选中的分区中搜索
 * @param[in] ctx SPANN 上下文
 * @param[in] index 底层索引
 * @param[in] query 查询向量 (dims,)
 * @param[in] k 返回结果数量
 * @param[in] search_list_size 搜索候选大小
 * @param[in] max_iterations 最大迭代次数
 * @param[in] selected_partitions 选中的分区 ID 数组
 * @param[in] selected_count 选中的分区数量
 * @param[out] distances 距离数组 (k,)
 * @param[out] labels 标签数组 (k,)
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_spann_search(diskann_spann_context_t *ctx,
                             const diskann_t *index,
                             const float *query,
                             int32_t k,
                             int32_t search_list_size,
                             int32_t max_iterations,
                             const int32_t *selected_partitions,
                             int32_t selected_count,
                             float *distances,
                             int32_t *labels);

/**
 * @brief 合并多个分区的搜索结果
 * @param[in] ctx SPANN 上下文
 * @param[in] partition_results 各分区的搜索结果
 * @param[in] partition_count 分区数量
 * @param[in] k 返回结果数量
 * @param[out] final_distances 最终距离数组 (k,)
 * @param[out] final_labels 最终标签数组 (k,)
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_spann_merge_results(diskann_spann_context_t *ctx,
                                    float **partition_results,
                                    int32_t partition_count,
                                    int32_t k,
                                    float *final_distances,
                                    int32_t *final_labels);

/**
 * @brief 持久化分区元数据
 * @param[in] ctx SPANN 上下文
 * @param[in] file 输出文件
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_spann_persist(diskann_spann_context_t *ctx, FILE *file);

/**
 * @brief 加载分区元数据
 * @param[in] config SPANN 配置
 * @param[in] file 输入文件
 * @return SPANN 上下文，失败返回 NULL
 */
diskann_spann_context_t *diskann_spann_load(const diskann_spann_config_t *config, FILE *file);

/**
 * @brief 获取分区数量
 * @param[in] ctx SPANN 上下文
 * @return 分区数量
 */
int32_t diskann_spann_get_partition_count(const diskann_spann_context_t *ctx);

/**
 * @brief 获取分区信息
 * @param[in] ctx SPANN 上下文
 * @param[in] partition_id 分区 ID
 * @param[out] center 分区中心 (dims,)
 * @param[out] count 向量数量
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_spann_get_partition_info(const diskann_spann_context_t *ctx,
                                         int32_t partition_id,
                                         float **center,
                                         int32_t *count);

#ifdef __cplusplus
}
#endif

#endif /* DISKANN_SPANN_H */