/**
 * @file graph_property.h
 * @brief 属性图存储头文件
 *
 * 实现属性图的顶点属性存储、边属性存储、标签系统和 Schema 验证
 */
#ifndef DB_GRAPH_PROPERTY_H
#define DB_GRAPH_PROPERTY_H

#include "db/graph/graph.h"
#include "db/graph/types.h"
#include "db/mm_pool.h"
#include <uthash/uthash.h>
#include <uthash/utlist.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 属性类型定义
 * ======================================================================== */

/** 属性值类型 */
typedef enum GraphPropType_e {
    GRAPH_PROP_NULL = 0,
    GRAPH_PROP_BOOL = 1,
    GRAPH_PROP_INT = 2,
    GRAPH_PROP_INT64 = 3,
    GRAPH_PROP_DOUBLE = 4,
    GRAPH_PROP_STRING = 5,
    GRAPH_PROP_BYTES = 6,
    GRAPH_PROP_TIMESTAMP = 7,
    GRAPH_PROP_VECTOR = 8,
    GRAPH_PROP_LIST = 9,
    GRAPH_PROP_MAP = 10
} GraphPropType;

/** 属性值 */
typedef struct GraphPropValue_s {
    GraphPropType type;
    union {
        bool bool_val;
        int64_t int_val;
        double double_val;
        char *string_val;
        struct {
            uint8_t *data;
            size_t len;
        } bytes_val;
        int64_t timestamp_val;
        struct {
            float *data;
            int dim;
        } vector_val;
        struct GraphPropList_s *list_val;
        struct GraphPropMap_s *map_val;
    } val;
} GraphPropValue;

/** 属性列表 */
typedef struct GraphPropList_s {
    GraphPropValue *values;
    size_t count;
    size_t capacity;
} GraphPropList;

/** 属性映射 */
typedef struct GraphPropMap_s {
    char **keys;
    GraphPropValue *values;
    size_t count;
    size_t capacity;
} GraphPropMap;

/** 单个属性 */
typedef struct GraphProperty_s {
    char *name;              /**< 属性名 */
    GraphPropValue value;    /**< 属性值 */
} GraphProperty;

/* ========================================================================
 * Schema 定义
 * ======================================================================== */

/** Schema 约束类型 */
typedef enum GraphSchemaConstraint_e {
    GRAPH_CONSTRAINT_NONE = 0,
    GRAPH_CONSTRAINT_REQUIRED = 1,    /**< 必须存在 */
    GRAPH_CONSTRAINT_UNIQUE = 2,      /**< 唯一 */
    GRAPH_CONSTRAINT_INDEX = 4,       /**< 索引 */
    GRAPH_CONSTRAINT_MIN = 8,         /**< 最小值 */
    GRAPH_CONSTRAINT_MAX = 16,        /**< 最大值 */
    GRAPH_CONSTRAINT_PATTERN = 32,    /**< 正则匹配 */
    GRAPH_CONSTRAINT_ENUM = 64        /**< 枚举值 */
} GraphSchemaConstraint;

/** Schema 属性定义 */
typedef struct GraphSchemaProp_s {
    char *name;                      /**< 属性名 */
    GraphPropType type;              /**< 类型 */
    uint32_t constraints;            /**< 约束位掩码 */
    union {
        int64_t min_int;
        double min_double;
        char *pattern;
        struct {
            GraphPropValue *values;
            size_t count;
        } enum_values;
    } constraint_data;
} GraphSchemaProp;

/** Schema 定义 */
typedef struct GraphSchema_s {
    char label_name[128];            /**< 标签名 */
    GraphSchemaProp *props;          /**< 属性定义 */
    size_t num_props;                /**< 属性数量 */
    bool is_vertex_schema;           /**< true=顶点, false=边 */
} GraphSchema;

/** Schema 管理器 */
typedef struct GraphSchemaMgr_s {
    GraphSchema *vertex_schemas;      /**< 顶点 Schema 数组 */
    size_t num_vertex_schemas;
    GraphSchema *edge_schemas;       /**< 边 Schema 数组 */
    size_t num_edge_schemas;
    void *mem_pool;                 /**< 内存池 */
} GraphSchemaMgr;

/* ========================================================================
 * 顶点属性存储
 * ======================================================================== */

/** 顶点属性记录 */
typedef struct GraphVertexProp_s {
    graph_vertex_id_t vid;           /**< 顶点 ID */
    GraphProperty *props;            /**< 属性数组 */
    size_t num_props;               /**< 属性数量 */
    graph_label_id_t *labels;       /**< 标签 ID 数组 */
    size_t num_labels;              /**< 标签数量 */
    uint64_t version;               /**< 版本号（用于 MVCC） */
    UT_hash_handle hh;              /**< UTHash 句柄 */
} GraphVertexProp;

/** 顶点属性存储 */
typedef struct GraphVertexStore_s {
    GraphVertexProp *vertices;      /**< 顶点哈希表 */
    graph_vertex_id_t next_vid;     /**< 下一个可用顶点 ID */
    graph_label_id_t *vid_by_label; /**< 按标签索引的顶点 */
    void *mem_pool;                /**< 内存池 */
    uint64_t version;              /**< 存储版本 */
} GraphVertexStore;

/**
 * @brief 创建顶点属性存储
 */
GraphVertexStore *graph_vertex_store_create(void *mem_pool);

/**
 * @brief 销毁顶点属性存储
 */
void graph_vertex_store_destroy(GraphVertexStore *store);

/**
 * @brief 添加顶点属性
 */
int graph_vertex_store_add(GraphVertexStore *store,
                          graph_vertex_id_t vid,
                          const GraphProperty *props,
                          size_t num_props,
                          const graph_label_id_t *labels,
                          size_t num_labels);

/**
 * @brief 获取顶点属性
 */
GraphVertexProp *graph_vertex_store_get(GraphVertexStore *store,
                                         graph_vertex_id_t vid);

/**
 * @brief 更新顶点属性
 */
int graph_vertex_store_update(GraphVertexStore *store,
                             graph_vertex_id_t vid,
                             const GraphProperty *props,
                             size_t num_props);

/**
 * @brief 删除顶点属性
 */
int graph_vertex_store_delete(GraphVertexStore *store, graph_vertex_id_t vid);

/**
 * @brief 按标签查询顶点
 */
graph_vertex_id_t *graph_vertex_store_get_by_label(GraphVertexStore *store,
                                                   graph_label_id_t label,
                                                   size_t *out_count);

/**
 * @brief 按属性查询顶点
 */
graph_vertex_id_t *graph_vertex_store_query(GraphVertexStore *store,
                                            const char *prop_name,
                                            const GraphPropValue *value,
                                            size_t *out_count);

/**
 * @brief 获取顶点数量
 */
uint64_t graph_vertex_store_count(const GraphVertexStore *store);

/* ========================================================================
 * 边属性存储
 * ======================================================================== */

/** 边属性记录 */
typedef struct GraphEdgeProp_s {
    graph_edge_id_t eid;            /**< 边 ID */
    graph_vertex_id_t src;          /**< 源顶点 */
    graph_vertex_id_t dst;          /**< 目标顶点 */
    graph_label_id_t rel_type;      /**< 关系类型 */
    GraphProperty *props;           /**< 属性数组 */
    size_t num_props;               /**< 属性数量 */
    uint64_t version;               /**< 版本号 */
    UT_hash_handle hh;              /**< UTHash 句柄 */
} GraphEdgeProp;

/** 边属性存储 */
typedef struct GraphEdgeStore_s {
    GraphEdgeProp *edges;           /**< 边哈希表 */
    graph_edge_id_t next_eid;       /**< 下一个可用边 ID */

    /** 按关系类型索引的边 */
    struct {
        graph_edge_id_t *edge_ids;
        size_t count;
        UT_hash_handle hh;
    } *edges_by_reltype;

    /** 按源顶点索引的边 */
    struct {
        graph_edge_id_t *edge_ids;
        size_t count;
        UT_hash_handle hh;
    } *edges_by_src;

    /** 按目标顶点索引的边 */
    struct {
        graph_edge_id_t *edge_ids;
        size_t count;
        UT_hash_handle hh;
    } *edges_by_dst;

    void *mem_pool;                /**< 内存池 */
    uint64_t version;              /**< 存储版本 */
} GraphEdgeStore;

/**
 * @brief 创建边属性存储
 */
GraphEdgeStore *graph_edge_store_create(void *mem_pool);

/**
 * @brief 销毁边属性存储
 */
void graph_edge_store_destroy(GraphEdgeStore *store);

/**
 * @brief 添加边属性
 */
int graph_edge_store_add(GraphEdgeStore *store,
                        graph_edge_id_t eid,
                        graph_vertex_id_t src,
                        graph_vertex_id_t dst,
                        graph_label_id_t rel_type,
                        const GraphProperty *props,
                        size_t num_props);

/**
 * @brief 获取边属性
 */
GraphEdgeProp *graph_edge_store_get(GraphEdgeStore *store, graph_edge_id_t eid);

/**
 * @brief 更新边属性
 */
int graph_edge_store_update(GraphEdgeStore *store,
                            graph_edge_id_t eid,
                            const GraphProperty *props,
                            size_t num_props);

/**
 * @brief 删除边
 */
int graph_edge_store_delete(GraphEdgeStore *store, graph_edge_id_t eid);

/**
 * @brief 获取顶点的出边
 */
graph_edge_id_t *graph_edge_store_get_out_edges(GraphEdgeStore *store,
                                                graph_vertex_id_t src,
                                                graph_label_id_t rel_type,
                                                size_t *out_count);

/**
 * @brief 获取顶点的入边
 */
graph_edge_id_t *graph_edge_store_get_in_edges(GraphEdgeStore *store,
                                               graph_vertex_id_t dst,
                                               graph_label_id_t rel_type,
                                               size_t *out_count);

/**
 * @brief 获取边数量
 */
uint64_t graph_edge_store_count(const GraphEdgeStore *store);

/* ========================================================================
 * 标签系统
 * ======================================================================== */

/** 标签信息 */
typedef struct GraphLabelInfo_s {
    graph_label_id_t id;           /**< 标签 ID */
    char name[128];                /**< 标签名 */
    bool is_vertex_label;          /**< true=顶点标签, false=边标签 */
    uint64_t vertex_count;         /**< 使用此标签的顶点数 */
    uint64_t edge_count;           /**< 使用此标签的边数 */
    UT_hash_handle hh;             /**< UTHash 句柄 */
} GraphLabelInfo;

/** 标签管理器 */
typedef struct GraphLabelMgr_s {
    GraphLabelInfo *labels_by_name; /**< 按名称索引 */
    GraphLabelInfo **labels_by_id;  /**< 按 ID 索引 */
    size_t num_labels;             /**< 标签数量 */
    graph_label_id_t next_label_id; /**< 下一个可用标签 ID */
    void *mem_pool;                /**< 内存池 */
} GraphLabelMgr;

/**
 * @brief 创建标签管理器
 */
GraphLabelMgr *graph_label_mgr_create(void *mem_pool);

/**
 * @brief 销毁标签管理器
 */
void graph_label_mgr_destroy(GraphLabelMgr *mgr);

/**
 * @brief 创建顶点标签
 */
graph_label_id_t graph_label_mgr_create_vertex_label(GraphLabelMgr *mgr,
                                                      const char *name);

/**
 * @brief 创建边标签
 */
graph_label_id_t graph_label_mgr_create_edge_label(GraphLabelMgr *mgr,
                                                   const char *name);

/**
 * @brief 获取或创建顶点标签
 */
graph_label_id_t graph_label_mgr_get_or_create_vertex_label(GraphLabelMgr *mgr,
                                                             const char *name);

/**
 * @brief 获取或创建边标签
 */
graph_label_id_t graph_label_mgr_get_or_create_edge_label(GraphLabelMgr *mgr,
                                                           const char *name);

/**
 * @brief 按名称获取标签
 */
GraphLabelInfo *graph_label_mgr_get_by_name(GraphLabelMgr *mgr, const char *name);

/**
 * @brief 按 ID 获取标签
 */
GraphLabelInfo *graph_label_mgr_get_by_id(GraphLabelMgr *mgr, graph_label_id_t id);

/**
 * @brief 列出所有顶点标签
 */
graph_label_id_t *graph_label_mgr_list_vertex_labels(GraphLabelMgr *mgr,
                                                      size_t *out_count);

/**
 * @brief 列出所有边标签
 */
graph_label_id_t *graph_label_mgr_list_edge_labels(GraphLabelMgr *mgr,
                                                   size_t *out_count);

/**
 * @brief 统计标签使用数
 */
void graph_label_mgr_inc_vertex_count(GraphLabelMgr *mgr, graph_label_id_t label);
void graph_label_mgr_dec_vertex_count(GraphLabelMgr *mgr, graph_label_id_t label);
void graph_label_mgr_inc_edge_count(GraphLabelMgr *mgr, graph_label_id_t label);
void graph_label_mgr_dec_edge_count(GraphLabelMgr *mgr, graph_label_id_t label);

/* ========================================================================
 * Schema 验证
 * ======================================================================== */

/**
 * @brief 创建 Schema 管理器
 */
GraphSchemaMgr *graph_schema_mgr_create(void *mem_pool);

/**
 * @brief 销毁 Schema 管理器
 */
void graph_schema_mgr_destroy(GraphSchemaMgr *mgr);

/**
 * @brief 添加顶点 Schema
 */
int graph_schema_mgr_add_vertex_schema(GraphSchemaMgr *mgr,
                                       const GraphSchema *schema);

/**
 * @brief 添加边 Schema
 */
int graph_schema_mgr_add_edge_schema(GraphSchemaMgr *mgr,
                                      const GraphSchema *schema);

/**
 * @brief 获取顶点 Schema
 */
GraphSchema *graph_schema_mgr_get_vertex_schema(GraphSchemaMgr *mgr,
                                                 const char *label_name);

/**
 * @brief 获取边 Schema
 */
GraphSchema *graph_schema_mgr_get_edge_schema(GraphSchemaMgr *mgr,
                                               const char *rel_type_name);

/**
 * @brief 验证顶点属性
 */
int graph_schema_validate_vertex(GraphSchemaMgr *mgr,
                                 const char *label_name,
                                 const GraphProperty *props,
                                 size_t num_props,
                                 char *error_buf,
                                 size_t error_buf_size);

/**
 * @brief 验证边属性
 */
int graph_schema_validate_edge(GraphSchemaMgr *mgr,
                               const char *rel_type_name,
                               const GraphProperty *props,
                               size_t num_props,
                               char *error_buf,
                               size_t error_buf_size);

/* ========================================================================
 * 属性值操作工具
 * ======================================================================== */

/**
 * @brief 复制属性值到目标
 */
int graph_prop_value_copy_to(const GraphPropValue *src, GraphPropValue *dst);

/**
 * @brief 创建属性值副本
 */
GraphPropValue *graph_prop_value_copy(const GraphPropValue *val, void *mem_pool);

/**
 * @brief 释放属性值
 */
void graph_prop_value_free(GraphPropValue *val);

/**
 * @brief 比较两个属性值
 */
int graph_prop_value_compare(const GraphPropValue *a, const GraphPropValue *b);

/**
 * @brief 属性值转字符串
 */
char *graph_prop_value_to_string(const GraphPropValue *val);

/**
 * @brief 获取属性类型名称
 */
const char *graph_prop_type_name(GraphPropType type);

/**
 * @brief 解析属性类型名称
 */
GraphPropType graph_parse_prop_type(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* DB_GRAPH_PROPERTY_H */
