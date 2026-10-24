/**
 * @file mmdb_types.h
 * @brief 多模态 SDK 公共类型定义
 */
#ifndef SDK_MMDB_TYPES_H
#define SDK_MMDB_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 不透明句柄 */
typedef struct mmdb_s mmdb_t;
typedef struct mmdb_collection_s mmdb_collection_t;
/* 注：mmdb_result_t 在本文件中以可见结构体形式定义（非不透明），无需前置声明 */

/* 数据模型枚举 */
typedef enum {
    MMDB_MODEL_VECTOR = 0,
    MMDB_MODEL_GRAPH = 1,
    MMDB_MODEL_TIMESERIES = 2,
    MMDB_MODEL_TEXT = 3,
} mmdb_model_t;

/* 字段类型 */
typedef enum {
    MMDB_TYPE_INT = 0,
    MMDB_TYPE_FLOAT = 1,
    MMDB_TYPE_TEXT = 2,
    MMDB_TYPE_BLOB = 3,
    MMDB_TYPE_VECTOR = 4,
    MMDB_TYPE_NODE = 5,
    MMDB_TYPE_EDGE = 6,
    MMDB_TYPE_DATAPOINT = 7,
} mmdb_data_type_t;

/* 数据库配置选项 */
typedef struct {
    int32_t cache_size_kb;
    int32_t busy_timeout_ms;
    int     enable_wal;
    int     verbose;
} mmdb_options_t;

/* mmdb_options_t 默认值（C99 指定初始化，0/缺省即安全） */
#define MMDB_OPTIONS_DEFAULT { 8192, 5000, 1, 0 }

/* Schema 字段定义 */
typedef struct {
    const char*     name;
    mmdb_data_type_t type;
    int             nullable;
    const char*     default_value_json;
} mmdb_field_def_t;

/* Collection Schema */
typedef struct {
    mmdb_model_t     model;
    size_t           field_count;
    mmdb_field_def_t* fields;
    size_t           vector_dim;
} mmdb_schema_t;

/* 向量条目 */
typedef struct {
    const uint8_t*  id;
    size_t          id_len;
    const float*    vector;
    size_t          dim;
    const char*     metadata_json;
    const char*     text;
} mmdb_vector_t;

/* 向量查询 */
typedef struct {
    const float*    query_vector;
    size_t          dim;
    size_t          top_k;
    const char*     filter_json;
    /* P6-M1.1 分页支持 */
    uint32_t        offset;       /* 结果偏移（从 0 开始），默认 0 */
    uint32_t        limit;        /* 返回最大数量（0 = 无限制），默认 0 */
} mmdb_query_t;

/* 结果项 */
typedef struct {
    uint8_t* id;
    size_t   id_len;
    float    distance;
    char*    metadata_json;
    char*    text;
} mmdb_result_item_t;

/* 结果集合 */
typedef struct {
    size_t             count;
    mmdb_result_item_t* items;
    /* P6-M1.1 分页元数据 */
    uint32_t           total_count;  /* 满足条件的总结果数 */
    bool               has_more;     /* 是否还有更多结果 */
    uint32_t           returned;     /* 本次返回的结果数 */
} mmdb_result_t;

/* 图节点 */
typedef struct {
    const char* id;
    const char* label;
    const char* properties_json;
} mmdb_node_t;

/* 图边 */
typedef struct {
    const char* source_id;
    const char* target_id;
    const char* label;
    double      weight;
    const char* properties_json;
} mmdb_edge_t;

/* 路径节点 */
typedef struct {
    const char* node_id;
    const char* label;
    const char* properties_json;
} mmdb_path_node_t;

/* 路径 */
typedef struct {
    size_t            node_count;
    mmdb_path_node_t* nodes;
    size_t            edge_count;
    mmdb_edge_t*      edges;
} mmdb_path_t;

/* 时序数据点 */
typedef struct {
    int64_t     timestamp;
    double      value;
    const char* tags_json;
} mmdb_datapoint_t;

/* 时序查询 */
typedef struct {
    int64_t     start;
    int64_t     end;
    const char* agg;
    const char* filter_json;
} mmdb_ts_query_t;

/* 文本条目 */
typedef struct {
    const char* id;
    const char* text;
    const char* metadata_json;
} mmdb_text_entry_t;

/* 文本查询 */
typedef struct {
    const char* query;
    size_t      top_k;
    const char* filter_json;
} mmdb_text_query_t;

#ifdef __cplusplus
}
#endif

#endif /* SDK_MMDB_TYPES_H */
