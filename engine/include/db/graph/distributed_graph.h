/**
 * @file distributed_graph.h
 * @brief 分布式图存储接口
 *
 * Phase12 - 实现分布式图存储，追赶 NebulaGraph 水平。
 */
#ifndef DB_GRAPH_DISTRIBUTED_H
#define DB_GRAPH_DISTRIBUTED_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 分布式图配置 */
typedef struct {
    uint32_t num_partitions;    /**< 分区数 */
    uint32_t num_replicas;     /**< 副本数 */
    bool enable_partition;       /**< 启用分区 */
} distributed_graph_config_t;

/** 分布式图不透明类型 */
typedef struct distributed_graph distributed_graph_t;

/** 创建分布式图 */
distributed_graph_t *distributed_graph_create(const distributed_graph_config_t *config);

/** 关闭分布式图 */
void distributed_graph_close(distributed_graph_t *graph);

/** 添加顶点 */
int distributed_graph_add_vertex(distributed_graph_t *graph, uint64_t vertex_id, const char *label);

/** 添加边 */
int distributed_graph_add_edge(distributed_graph_t *graph, uint64_t src, uint64_t dst, const char *type);

/** 获取顶点数 */
uint64_t distributed_graph_get_vertex_count(const distributed_graph_t *graph);

/** 获取边数 */
uint64_t distributed_graph_get_edge_count(const distributed_graph_t *graph);

#ifdef __cplusplus
}
#endif

#endif /* DB_GRAPH_DISTRIBUTED_H */
