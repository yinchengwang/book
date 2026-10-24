/**
 * @file graph_distributed.h
 * @brief 分布式图存储接口
 *
 * Phase12 - 实现分布式图存储，追赶 NebulaGraph/Neo4j 水平。
 *
 * 设计目标：
 * - 支持千亿边规模
 * - 图分区策略：基于顶点ID哈希
 * - 图复制：多副本容错
 * - 图遍历优化：减少跨分区边访问
 */
#ifndef DB_GRAPH_DISTRIBUTED_H
#define DB_GRAPH_DISTRIBUTED_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 最大分区数 */
#define GRAPH_MAX_PARTITIONS 256

/** 最大副本数 */
#define GRAPH_MAX_REPLICAS 5

/** 默认分区数 */
#define GRAPH_DEFAULT_NUM_PARTITIONS 4

/** 默认副本数 */
#define GRAPH_DEFAULT_NUM_REPLICAS 3

/* ========================================================================
 * 图分区策略
 * ======================================================================== */

typedef enum {
    PARTITION_BY_HASH = 0,      /**< 基于顶点ID哈希 */
    PARTITION_BY_RANGE = 1,     /**< 基于顶点ID范围 */
    PARTITION_BY_LABEL = 2,     /**< 基于顶点标签 */
    PARTITION_BY_EDGE = 3       /**< 基于边类型 */
} graph_partition_strategy_t;

/* ========================================================================
 * 副本状态
 * ======================================================================== */

typedef enum {
    REPLICA_PRIMARY = 0,       /**< 主副本 */
    REPLICA_SECONDARY = 1,     /**< 从副本 */
    REPLICA_CATCHING_UP = 2   /**< 追赶中 */
} graph_replica_state_t;

/* ========================================================================
 * 图分区信息
 * ======================================================================== */

typedef struct {
    uint32_t partition_id;
    uint64_t start_vertex_id;
    uint64_t end_vertex_id;
    char address[256];
    uint16_t port;
    bool is_local;
    uint64_t num_vertices;
    uint64_t num_edges;
    size_t size_bytes;
} graph_partition_info_t;

/* ========================================================================
 * 图副本信息
 * ======================================================================== */

typedef struct {
    uint32_t replica_id;
    uint32_t partition_id;
    graph_replica_state_t state;
    char address[256];
    uint16_t port;
    uint64_t commit_index;
    uint64_t last_sync_time;
} graph_replica_info_t;

/* ========================================================================
 * 图存储配置
 * ======================================================================== */

typedef struct {
    uint32_t num_partitions;              /**< 分区数 */
    uint32_t num_replicas;               /**< 副本数 */
    graph_partition_strategy_t strategy;   /**< 分区策略 */
    size_t vertex_cache_size;             /**< 顶点缓存大小 */
    size_t edge_cache_size;              /**< 边缓存大小 */
    bool enable_edge_index;              /**< 启用边索引 */
    bool enable_property_index;          /**< 启用属性索引 */
} graph_distributed_config_t;

/* ========================================================================
 * 图存储不透明类型
 * ======================================================================== */

typedef struct graph_distributed graph_distributed_t;

/* ========================================================================
 * 顶点/边操作
 * ======================================================================== */

/**
 * @brief 创建顶点
 *
 * @param graph 图
 * @param vertex_id 顶点ID
 * @param label 标签
 * @param properties 属性（JSON）
 * @return 0 成功
 */
int graph_distributed_add_vertex(graph_distributed_t *graph,
                               uint64_t vertex_id,
                               const char *label,
                               const char *properties);

/**
 * @brief 创建边
 *
 * @param graph 图
 * @param src_vertex 源顶点
 * @param dst_vertex 目标顶点
 * @param edge_type 边类型
 * @param properties 属性
 * @return 0 成功
 */
int graph_distributed_add_edge(graph_distributed_t *graph,
                              uint64_t src_vertex,
                              uint64_t dst_vertex,
                              const char *edge_type,
                              const char *properties);

/**
 * @brief 删除顶点及其所有边
 *
 * @param graph 图
 * @param vertex_id 顶点ID
 * @return 0 成功
 */
int graph_distributed_remove_vertex(graph_distributed_t *graph, uint64_t vertex_id);

/**
 * @brief 删除边
 *
 * @param graph 图
 * @param src_vertex 源顶点
 * @param dst_vertex 目标顶点
 * @param edge_type 边类型
 * @return 0 成功
 */
int graph_distributed_remove_edge(graph_distributed_t *graph,
                                 uint64_t src_vertex,
                                 uint64_t dst_vertex,
                                 const char *edge_type);

/**
 * @brief 更新顶点属性
 *
 * @param graph 图
 * @param vertex_id 顶点ID
 * @param properties 新属性
 * @return 0 成功
 */
int graph_distributed_update_vertex(graph_distributed_t *graph,
                                   uint64_t vertex_id,
                                   const char *properties);

/**
 * @brief 更新边属性
 *
 * @param graph 图
 * @param src_vertex 源顶点
 * @param dst_vertex 目标顶点
 * @param edge_type 边类型
 * @param properties 新属性
 * @return 0 成功
 */
int graph_distributed_update_edge(graph_distributed_t *graph,
                                 uint64_t src_vertex,
                                 uint64_t dst_vertex,
                                 const char *edge_type,
                                 const char *properties);

/* ========================================================================
 * 图遍历操作
 * ======================================================================== */

/** 遍历结果 */
typedef struct {
    uint64_t *vertex_ids;
    size_t num_vertices;
    size_t capacity;
} graph_traversal_result_t;

/**
 * @brief K跳邻居查询
 *
 * @param graph 图
 * @param start_vertex 起始顶点
 * @param k 跳数
 * @param direction 方向 (0=both, 1=out, 2=in)
 * @return 遍历结果
 */
graph_traversal_result_t *graph_distributed_k_hop(graph_distributed_t *graph,
                                                  uint64_t start_vertex,
                                                  uint32_t k,
                                                  int direction);

/**
 * @brief 最短路径查询
 *
 * @param graph 图
 * @param src 源顶点
 * @param dst 目标顶点
 * @param max_hops 最大跳数
 * @return 路径顶点数组（NULL 表示无路径）
 */
uint64_t *graph_distributed_shortest_path(graph_distributed_t *graph,
                                          uint64_t src,
                                          uint64_t dst,
                                          uint32_t max_hops,
                                          size_t *path_length);

/**
 * @brief 批量获取顶点
 *
 * @param graph 图
 * @param vertex_ids 顶点ID数组
 * @param count 数量
 * @param out_properties 输出属性数组
 * @return 成功获取的数量
 */
size_t graph_distributed_batch_get_vertices(graph_distributed_t *graph,
                                            const uint64_t *vertex_ids,
                                            size_t count,
                                            char ***out_properties);

/**
 * @brief 释放遍历结果
 */
void graph_distributed_free_traversal_result(graph_traversal_result_t *result);

/* ========================================================================
 * 图算法
 * ======================================================================== */

/** PageRank 结果 */
typedef struct {
    uint64_t vertex_id;
    double pagerank;
} graph_pagerank_result_t;

/**
 * @brief PageRank 计算
 *
 * @param graph 图
 * @param iterations 迭代次数
 * @param damping 阻尼系数
 * @param out_results 输出结果
 * @param out_count 结果数量
 * @return 0 成功
 */
int graph_distributed_pagerank(graph_distributed_t *graph,
                               uint32_t iterations,
                               double damping,
                               graph_pagerank_result_t **out_results,
                               size_t *out_count);

/**
 * @brief 连通分量检测
 *
 * @param graph 图
 * @param out_components 输出分量数组
 * @param out_count 分量数量
 * @return 0 成功
 */
int graph_distributed_connected_components(graph_distributed_t *graph,
                                          uint64_t **out_components,
                                          size_t *out_count);

/**
 * @brief 社区检测（Louvain算法）
 *
 * @param graph 图
 * @param out_communities 输出社区数组
 * @param out_count 社区数量
 * @return 0 成功
 */
int graph_distributed_community_detection(graph_distributed_t *graph,
                                         uint64_t **out_communities,
                                         size_t *out_count);

/* ========================================================================
 * 分区管理
 * ======================================================================== */

/**
 * @brief 获取分区信息
 */
const graph_partition_info_t *graph_distributed_get_partition_info(
    const graph_distributed_t *graph, uint32_t partition_id);

/**
 * @brief 获取所有分区信息
 */
graph_partition_info_t *graph_distributed_get_all_partitions(
    const graph_distributed_t *graph, uint32_t *out_count);

/**
 * @brief 添加分区
 */
int graph_distributed_add_partition(graph_distributed_t *graph,
                                   uint32_t partition_id,
                                   const char *address,
                                   uint16_t port);

/**
 * @brief 移除分区
 */
int graph_distributed_remove_partition(graph_distributed_t *graph,
                                       uint32_t partition_id);

/**
 * @brief 分区内重平衡
 */
int graph_distributed_rebalance_partition(graph_distributed_t *graph,
                                          uint32_t partition_id);

/**
 * @brief 获取分区数
 */
uint32_t graph_distributed_get_num_partitions(const graph_distributed_t *graph);

/* ========================================================================
 * 副本管理
 * ======================================================================== */

/**
 * @brief 获取副本信息
 */
const graph_replica_info_t *graph_distributed_get_replica_info(
    const graph_distributed_t *graph, uint32_t replica_id);

/**
 * @brief 添加副本
 */
int graph_distributed_add_replica(graph_distributed_t *graph,
                                   uint32_t partition_id,
                                   const char *address,
                                   uint16_t port);

/**
 * @brief 移除副本
 */
int graph_distributed_remove_replica(graph_distributed_t *graph, uint32_t replica_id);

/**
 * @brief 触发副本同步
 */
int graph_distributed_sync_replica(graph_distributed_t *graph, uint32_t replica_id);

/* ========================================================================
 * 统计信息
 * ======================================================================== */

/** 图统计信息 */
typedef struct {
    uint64_t num_vertices;
    uint64_t num_edges;
    uint32_t num_partitions;
    uint32_t num_replicas;
    size_t total_size_bytes;
    uint64_t avg_degree;
    double density;
} graph_distributed_stats_t;

/**
 * @brief 获取统计信息
 */
void graph_distributed_get_stats(const graph_distributed_t *graph,
                                  graph_distributed_stats_t *stats);

/* ========================================================================
 * 生命周期
 * ======================================================================== */

/**
 * @brief 创建分布式图存储
 */
graph_distributed_t *graph_distributed_create(const graph_distributed_config_t *config);

/**
 * @brief 打开已存在的图
 */
graph_distributed_t *graph_distributed_open(const char *data_dir);

/**
 * @brief 关闭图
 */
void graph_distributed_close(graph_distributed_t *graph);

/**
 * @brief 保存图到磁盘
 */
int graph_distributed_save(graph_distributed_t *graph);

/**
 * @brief 从磁盘加载
 */
int graph_distributed_load(graph_distributed_t *graph);

#ifdef __cplusplus
}
#endif

#endif /* DB_GRAPH_DISTRIBUTED_H */
