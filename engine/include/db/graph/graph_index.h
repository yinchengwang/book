/**
 * @file graph_index.h
 * @brief 图索引头文件
 *
 * 实现标签索引、属性索引和向量属性索引
 */
#ifndef DB_GRAPH_INDEX_H
#define DB_GRAPH_INDEX_H

#include "db/graph/graph.h"
#include "db/graph/graph_property.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 标签索引
 * ======================================================================== */

/** 顶点标签索引项 */
typedef struct GraphLabelIndexEntry_s {
    graph_label_id_t label_id;   /**< 标签 ID */
    graph_vertex_id_t *vertex_ids; /**< 顶点 ID 数组 */
    size_t num_vertices;        /**< 顶点数量 */
    size_t capacity;            /**< 容量 */
    UT_hash_handle hh;          /**< UTHash 句柄 */
} GraphLabelIndexEntry;

/** 边标签索引项 */
typedef struct GraphEdgeLabelIndexEntry_s {
    graph_label_id_t rel_type;  /**< 关系类型 ID */
    graph_edge_id_t *edge_ids;  /**< 边 ID 数组 */
    size_t num_edges;          /**< 边数量 */
    size_t capacity;            /**< 容量 */
    UT_hash_handle hh;          /**< UTHash 句柄 */
} GraphEdgeLabelIndexEntry;

/** 标签索引 */
typedef struct GraphLabelIndex_s {
    GraphLabelIndexEntry *vertex_index; /**< 顶点标签索引 */
    GraphEdgeLabelIndexEntry *edge_index; /**< 边标签索引 */
    void *mem_pool;                /**< 内存池 */
} GraphLabelIndex;

/**
 * @brief 创建标签索引
 */
GraphLabelIndex *graph_label_index_create(void *mem_pool);

/**
 * @brief 销毁标签索引
 */
void graph_label_index_destroy(GraphLabelIndex *index);

/**
 * @brief 添加顶点到标签索引
 */
int graph_label_index_add_vertex(GraphLabelIndex *index,
                                graph_vertex_id_t vid,
                                graph_label_id_t label);

/**
 * @brief 从标签索引移除顶点
 */
int graph_label_index_remove_vertex(GraphLabelIndex *index,
                                   graph_vertex_id_t vid,
                                   graph_label_id_t label);

/**
 * @brief 添加边到标签索引
 */
int graph_label_index_add_edge(GraphLabelIndex *index,
                              graph_edge_id_t eid,
                              graph_label_id_t rel_type);

/**
 * @brief 从标签索引移除边
 */
int graph_label_index_remove_edge(GraphLabelIndex *index,
                                 graph_edge_id_t eid,
                                 graph_label_id_t rel_type);

/**
 * @brief 按标签查询顶点
 */
graph_vertex_id_t *graph_label_index_get_vertices(GraphLabelIndex *index,
                                                  graph_label_id_t label,
                                                  size_t *out_count);

/**
 * @brief 按关系类型查询边
 */
graph_edge_id_t *graph_label_index_get_edges(GraphLabelIndex *index,
                                             graph_label_id_t rel_type,
                                             size_t *out_count);

/**
 * @brief 获取使用某标签的顶点数
 */
size_t graph_label_index_vertex_count(GraphLabelIndex *index, graph_label_id_t label);

/**
 * @brief 获取使用某关系类型的边数
 */
size_t graph_label_index_edge_count(GraphLabelIndex *index, graph_label_id_t rel_type);

/* ========================================================================
 * 属性索引
 * ======================================================================== */

/** 索引类型 */
typedef enum GraphIndexType_e {
    GRAPH_INDEX_BTREE,           /**< B-Tree 索引 */
    GRAPH_INDEX_HASH,            /**< Hash 索引 */
    GRAPH_INDEX_RANGE            /**< 范围索引 */
} GraphIndexType;

/** 属性索引项 */
typedef struct GraphPropIndexEntry_s {
    char prop_name[64];          /**< 属性名 */
    GraphPropValue min_value;    /**< 最小值 */
    GraphPropValue max_value;    /**< 最大值 */
    graph_vertex_id_t *vertex_ids; /**< 顶点 ID 数组 */
    graph_edge_id_t *edge_ids;   /**< 边 ID 数组 */
    size_t num_vertices;
    size_t num_edges;
    UT_hash_handle hh;           /**< UTHash 句柄 */
} GraphPropIndexEntry;

/** 属性值到顶点 ID 的映射项 */
typedef struct GraphPropValueEntry_s {
    GraphPropValue value;        /**< 属性值 */
    graph_vertex_id_t *vertex_ids; /**< 顶点 ID 数组 */
    size_t num_vertices;
    size_t capacity;
    UT_hash_handle hh;           /**< UTHash 句柄 */
} GraphPropValueEntry;

/** 属性索引 */
typedef struct GraphPropIndex_s {
    char index_name[128];        /**< 索引名 */
    char prop_name[64];          /**< 索引的属性名 */
    GraphIndexType type;         /**< 索引类型 */
    bool is_vertex_index;         /**< true=顶点索引, false=边索引 */
    GraphPropValueEntry *value_index; /**< 值索引（Hash 索引） */
    GraphPropIndexEntry **range_indexes; /**< 范围索引（B-Tree） */
    size_t num_range_indexes;
    void *mem_pool;
} GraphPropIndex;

/** 属性索引管理器 */
typedef struct GraphPropIndexMgr_s {
    GraphPropIndex **indexes;    /**< 索引数组 */
    size_t num_indexes;         /**< 索引数量 */
    void *mem_pool;
} GraphPropIndexMgr;

/**
 * @brief 创建属性索引管理器
 */
GraphPropIndexMgr *graph_prop_index_mgr_create(void *mem_pool);

/**
 * @brief 销毁属性索引管理器
 */
void graph_prop_index_mgr_destroy(GraphPropIndexMgr *mgr);

/**
 * @brief 创建属性索引
 */
int graph_prop_index_create(GraphPropIndexMgr *mgr,
                           const char *index_name,
                           const char *prop_name,
                           GraphIndexType type,
                           bool is_vertex_index);

/**
 * @brief 删除属性索引
 */
int graph_prop_index_drop(GraphPropIndexMgr *mgr, const char *index_name);

/**
 * @brief 获取属性索引
 */
GraphPropIndex *graph_prop_index_get(GraphPropIndexMgr *mgr, const char *index_name);

/**
 * @brief 向索引添加顶点
 */
int graph_prop_index_add_vertex(GraphPropIndexMgr *mgr,
                               const char *index_name,
                               graph_vertex_id_t vid,
                               const GraphPropValue *value);

/**
 * @brief 向索引添加边
 */
int graph_prop_index_add_edge(GraphPropIndexMgr *mgr,
                             const char *index_name,
                             graph_edge_id_t eid,
                             const GraphPropValue *value);

/**
 * @brief 从索引移除顶点
 */
int graph_prop_index_remove_vertex(GraphPropIndexMgr *mgr,
                                  const char *index_name,
                                  graph_vertex_id_t vid);

/**
 * @brief 从索引移除边
 */
int graph_prop_index_remove_edge(GraphPropIndexMgr *mgr,
                               const char *index_name,
                               graph_edge_id_t eid);

/**
 * @brief 精确查询
 */
graph_vertex_id_t *graph_prop_index_lookup(GraphPropIndexMgr *mgr,
                                          const char *index_name,
                                          const GraphPropValue *value,
                                          size_t *out_count);

/**
 * @brief 范围查询（>=）
 */
graph_vertex_id_t *graph_prop_index_range_ge(GraphPropIndexMgr *mgr,
                                            const char *index_name,
                                            const GraphPropValue *min_value,
                                            size_t *out_count);

/**
 * @brief 范围查询（<=）
 */
graph_vertex_id_t *graph_prop_index_range_le(GraphPropIndexMgr *mgr,
                                            const char *index_name,
                                            const GraphPropValue *max_value,
                                            size_t *out_count);

/**
 * @brief 范围查询（between）
 */
graph_vertex_id_t *graph_prop_index_range_between(GraphPropIndexMgr *mgr,
                                                 const char *index_name,
                                                 const GraphPropValue *min_value,
                                                 const GraphPropValue *max_value,
                                                 size_t *out_count);

/* ========================================================================
 * 向量属性索引
 * ======================================================================== */

/** 向量距离度量 */
typedef enum GraphVectorMetric_e {
    GRAPH_VECTOR_L2,             /**< 欧氏距离 */
    GRAPH_VECTOR_COSINE,         /**< 余弦距离 */
    GRAPH_VECTOR_DOT            /**< 点积 */
} GraphVectorMetric;

/** 向量索引配置 */
typedef struct GraphVectorIndexConfig_s {
    GraphVectorMetric metric;    /**< 距离度量 */
    int num_clusters;           /**< 聚类数（用于 IVF） */
    int vectors_per_cluster;    /**< 每簇向量数 */
    bool use_compression;        /**< 是否使用压缩 */
} GraphVectorIndexConfig;

/** 向量属性索引 */
typedef struct GraphVectorIndex_s {
    char index_name[128];        /**< 索引名 */
    char prop_name[64];          /**< 属性名 */
    bool is_vertex_index;         /**< true=顶点索引 */
    GraphVectorIndexConfig config;
    void *index_impl;           /**< 底层索引实现（HNSW/IVF 等） */
    void *mem_pool;
} GraphVectorIndex;

/** 向量搜索结果 */
typedef struct GraphVectorSearchResult_s {
    graph_vertex_id_t vid;       /**< 顶点 ID */
    graph_edge_id_t eid;        /**< 边 ID（用于边索引） */
    float distance;             /**< 距离 */
} GraphVectorSearchResult;

/** 向量索引管理器 */
typedef struct GraphVectorIndexMgr_s {
    GraphVectorIndex **indexes;  /**< 索引数组 */
    size_t num_indexes;         /**< 索引数量 */
    void *mem_pool;
} GraphVectorIndexMgr;

/**
 * @brief 创建向量索引管理器
 */
GraphVectorIndexMgr *graph_vector_index_mgr_create(void *mem_pool);

/**
 * @brief 销毁向量索引管理器
 */
void graph_vector_index_mgr_destroy(GraphVectorIndexMgr *mgr);

/**
 * @brief 创建向量属性索引
 */
int graph_vector_index_create(GraphVectorIndexMgr *mgr,
                             const char *index_name,
                             const char *prop_name,
                             bool is_vertex_index,
                             const GraphVectorIndexConfig *config);

/**
 * @brief 删除向量属性索引
 */
int graph_vector_index_drop(GraphVectorIndexMgr *mgr, const char *index_name);

/**
 * @brief 添加向量到索引
 */
int graph_vector_index_add(GraphVectorIndexMgr *mgr,
                         const char *index_name,
                         graph_vertex_id_t vid,
                         const float *vector,
                         int dim);

/**
 * @brief 从索引移除向量
 */
int graph_vector_index_remove(GraphVectorIndexMgr *mgr,
                             const char *index_name,
                             graph_vertex_id_t vid);

/**
 * @brief 向量最近邻搜索
 */
GraphVectorSearchResult *graph_vector_index_search(GraphVectorIndexMgr *mgr,
                                                   const char *index_name,
                                                   const float *query_vector,
                                                   int query_dim,
                                                   int top_k,
                                                   size_t *out_count);

/**
 * @brief 带过滤的向量搜索
 */
GraphVectorSearchResult *graph_vector_index_search_filter(GraphVectorIndexMgr *mgr,
                                                        const char *index_name,
                                                        const float *query_vector,
                                                        int query_dim,
                                                        int top_k,
                                                        const graph_label_id_t *labels,
                                                        size_t num_labels,
                                                        size_t *out_count);

/**
 * @brief 释放搜索结果
 */
void graph_vector_search_result_free(GraphVectorSearchResult *results, size_t count);

/* ========================================================================
 * 复合索引
 * ======================================================================== */

/** 复合索引定义 */
typedef struct GraphCompositeIndexDef_s {
    char index_name[128];        /**< 索引名 */
    char **prop_names;           /**< 属性名数组 */
    size_t num_props;           /**< 属性数量 */
    bool is_unique;              /**< 是否唯一 */
    GraphIndexType type;         /**< 索引类型 */
} GraphCompositeIndexDef;

/** 复合索引 */
typedef struct GraphCompositeIndex_s {
    GraphCompositeIndexDef def;
    void *index_impl;           /**< 底层实现 */
    void *mem_pool;
} GraphCompositeIndex;

/**
 * @brief 创建复合索引
 */
GraphCompositeIndex *graph_composite_index_create(GraphCompositeIndexDef *def,
                                                 void *mem_pool);

/**
 * @brief 销毁复合索引
 */
void graph_composite_index_destroy(GraphCompositeIndex *index);

/**
 * @brief 添加记录到复合索引
 */
int graph_composite_index_insert(GraphCompositeIndex *index,
                                graph_vertex_id_t vid,
                                const GraphPropValue *values);

/**
 * @brief 从复合索引删除记录
 */
int graph_composite_index_delete(GraphCompositeIndex *index, graph_vertex_id_t vid);

/**
 * @brief 复合索引查询
 */
graph_vertex_id_t *graph_composite_index_lookup(GraphCompositeIndex *index,
                                                const GraphPropValue *values,
                                                size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif /* DB_GRAPH_INDEX_H */
