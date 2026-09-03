/*
 * diskann_merge.h
 *
 * DiskANN Merge-Vamana 子图合并头文件。
 * 支持将多个子图合并为全局图。
 */

#ifndef DISKANN_MERGE_H
#define DISKANN_MERGE_H

#include <stdbool.h>
#include <stdint.h>

#include <db/index/vector_index/diskann/diskann.h>
#include <db/index/vector_index/diskann/diskann_partition.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Merge-Vamana 上下文
 * ============================================================================ */

/**
 * @brief Merge-Vamana 上下文结构
 */
typedef struct diskann_merge_context {
    diskann_merge_config_t config;   /**< Merge 配置 */
    diskann_partition_result_t *partition_result; /**< 分区结果 */

    /* 中间状态 */
    int32_t *global_node_id_map;     /**< 原始 ID -> 全局 ID 映射 */
    int32_t *reverse_map;            /**< 全局 ID -> 原始 ID 映射 */
    int32_t global_node_count;       /**< 全局节点数量 */
} diskann_merge_context_t;

/* ============================================================================
 * Merge-Vamana 函数
 * ============================================================================ */

/**
 * @brief 创建 Merge 上下文
 * @param[in] config Merge 配置
 * @return 上下文指针，失败返回 NULL
 */
diskann_merge_context_t *diskann_merge_context_create(const diskann_merge_config_t *config);

/**
 * @brief 释放 Merge 上下文
 * @param[in] ctx Merge 上下文
 */
void diskann_merge_context_free(diskann_merge_context_t *ctx);

/**
 * @brief 验证 Merge 配置
 * @param[in] config 配置指针
 * @return 有效返回 1，无效返回 0
 */
int32_t diskann_merge_config_validate_ex(const diskann_merge_config_t *config);

/**
 * @brief 执行数据分区
 * @param[in] ctx Merge 上下文
 * @param[in] vectors 原始向量 (n * dims)
 * @param[in] n 向量数量
 * @param[in] dims 向量维度
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_merge_partition_data(diskann_merge_context_t *ctx,
                                     const float *vectors,
                                     int32_t n,
                                     int32_t dims);

/**
 * @brief 构建子图（每个分区独立构建）
 * @param[in] ctx Merge 上下文
 * @param[in] vectors 原始向量 (n * dims)
 * @param[in] n 向量数量
 * @param[in] dims 向量维度
 * @param[in] index_size 目标邻居数
 * @param[in] search_list_size 构图候选数
 * @param[out] subgraphs 子图数组指针（由函数分配）
 * @param[out] subgraph_count 子图数量
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_merge_build_subgraphs(diskann_merge_context_t *ctx,
                                      const float *vectors,
                                      int32_t n,
                                      int32_t dims,
                                      int32_t index_size,
                                      int32_t search_list_size,
                                      diskann_t ***subgraphs,
                                      int32_t *subgraph_count);

/**
 * @brief 合并多个子图
 * @param[in] ctx Merge 上下文
 * @param[in] subgraphs 子图数组
 * @param[in] count 子图数量
 * @param[in] vectors 原始向量 (n * dims)
 * @param[in] n 向量数量
 * @param[in] dims 向量维度
 * @param[out] merged 合并后的图
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_merge_graphs(diskann_merge_context_t *ctx,
                             diskann_t **subgraphs,
                             int32_t count,
                             const float *vectors,
                             int32_t n,
                             int32_t dims,
                             diskann_t **merged);

/**
 * @brief 节点去重：检测并标记重复节点
 * @param[in] ctx Merge 上下文
 * @param[in] subgraphs 子图数组
 * @param[in] count 子图数量
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_merge_deduplicate_nodes(diskann_merge_context_t *ctx,
                                        diskann_t **subgraphs,
                                        int32_t count);

/**
 * @brief 合并边：收集所有子图的边
 * @param[in] ctx Merge 上下文
 * @param[in] subgraphs 子图数组
 * @param[in] count 子图数量
 * @param[out] edge_count 总边数
 * @return 边数组（需要手动释放），失败返回 NULL
 */
int32_t *diskann_merge_collect_edges(diskann_merge_context_t *ctx,
                                     diskann_t **subgraphs,
                                     int32_t count,
                                     int32_t *edge_count);

/**
 * @brief 构建跨分区边
 * @param[in] ctx Merge 上下文
 * @param[in] merged 合并后的图
 * @param[in] vectors 原始向量
 * @param[in] n 向量数量
 * @param[in] dims 向量维度
 * @return 成功返回 0，失败返回 -1
 */
int32_t diskann_merge_build_cross_partition_edges(diskann_merge_context_t *ctx,
                                                  diskann_t *merged,
                                                  const float *vectors,
                                                  int32_t n,
                                                  int32_t dims);

/**
 * @brief 获取全局节点数
 * @param[in] ctx Merge 上下文
 * @return 全局节点数
 */
int32_t diskann_merge_get_global_node_count(const diskann_merge_context_t *ctx);

/**
 * @brief 原始 ID 转换为全局 ID
 * @param[in] ctx Merge 上下文
 * @param[in] original_id 原始 ID
 * @return 全局 ID，不存在返回 -1
 */
int32_t diskann_merge_to_global_id(const diskann_merge_context_t *ctx, int32_t original_id);

/**
 * @brief 全局 ID 转换为原始 ID
 * @param[in] ctx Merge 上下文
 * @param[in] global_id 全局 ID
 * @return 原始 ID
 */
int32_t diskann_merge_to_original_id(const diskann_merge_context_t *ctx, int32_t global_id);

#ifdef __cplusplus
}
#endif

#endif /* DISKANN_MERGE_H */