/*
 * HNSW 图边修复
 *
 * 在向量删除后修复图结构，确保图连通性和搜索质量。
 */

#ifndef HNSW_GRAPH_REPAIR_H
#define HNSW_GRAPH_REPAIR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 边修复统计
 */
typedef struct graph_repair_stats {
    int32_t deleted_count;     /* 已删除节点数 */
    int32_t repaired_count;     /* 修复的边数 */
    int32_t orphan_count;       /* 孤立节点数 */
    int32_t entry_updated;      /* 入口点更新次数 */
} graph_repair_stats_t;

/*
 * 边修复配置
 */
typedef struct graph_repair_config {
    int32_t max_repair_neighbors;  /* 每个节点最多修复的邻居数 */
    int32_t ef_search;              /* 搜索时使用的 ef 参数 */
    bool    update_entry_point;     /* 是否更新入口点 */
} graph_repair_config_t;

/**
 * 初始化默认边修复配置
 */
void graph_repair_config_init_default(graph_repair_config_t *config);

/**
 * 修复被删除节点的邻居边
 *
 * 对于被删除的节点，将其所有邻居从邻居列表中移除，
 * 并尝试为邻居补充新的连接以保持图的结构完整性。
 *
 * @param hnsw_index     HNSW 索引
 * @param deleted_ids    已删除的节点 ID 数组
 * @param n_deleted      已删除节点数量
 * @param config         修复配置（可为 NULL）
 * @param stats          统计输出（可为 NULL）
 * @return 0 成功，-1 失败
 */
int32_t graph_repair_deleted_neighbors(void *hnsw_index,
                                       const int32_t *deleted_ids,
                                       int32_t n_deleted,
                                       const graph_repair_config_t *config,
                                       graph_repair_stats_t *stats);

/**
 * 检查并修复孤立节点
 *
 * @param hnsw_index  HNSW 索引
 * @return 修复的孤立节点数
 */
int32_t graph_repair_orphans(void *hnsw_index);

/**
 * 更新入口点
 *
 * 如果当前入口点已被删除，选取一个新的有效入口点。
 *
 * @param hnsw_index  HNSW 索引
 * @return 0 成功，-1 失败
 */
int32_t graph_repair_update_entry_point(void *hnsw_index);

#ifdef __cplusplus
}
#endif

#endif /* HNSW_GRAPH_REPAIR_H */
