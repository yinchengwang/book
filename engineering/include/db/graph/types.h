/**
 * @file types.h
 * @brief 图数据库核心类型定义
 *
 * 定义顶点、边、属性等核心数据结构
 */
#ifndef DB_GRAPH_TYPES_H
#define DB_GRAPH_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 图对象标识符
 * ============================================================ */

/** 顶点 ID (64位) */
typedef uint64_t graph_vertex_id_t;

/** 边 ID (64位) */
typedef uint64_t graph_edge_id_t;

/** 标签 ID (32位) */
typedef uint32_t graph_label_id_t;

/** 属性键 ID (32位) */
typedef uint32_t graph_prop_key_id_t;

/** 无效 ID */
#define GRAPH_INVALID_ID ((uint64_t)0)

/* ============================================================
 * 属性值类型
 * ============================================================ */

/** 属性值类型 */
typedef enum graph_value_type_e {
    GRAPH_NULL = 0,       /**< NULL 值 */
    GRAPH_INT = 1,        /**< int64_t 整数 */
    GRAPH_FLOAT = 2,      /**< double 浮点数 */
    GRAPH_BOOL = 3,       /**< bool 布尔值 */
    GRAPH_STRING = 4,     /**< 字符串 */
    GRAPH_LIST = 5,       /**< 列表 */
    GRAPH_MAP = 6         /**< 映射 */
} graph_value_type_t;

/** 属性值联合体 */
typedef struct graph_value_s {
    graph_value_type_t type;   /**< 类型 */
    union {
        int64_t     int_val;       /**< 整数值 */
        double      float_val;     /**< 浮点值 */
        bool        bool_val;      /**< 布尔值 */
        struct {
            char    *str;          /**< 字符串指针 */
            size_t  len;           /**< 字符串长度 */
        } string_val;             /**< 字符串值 */
    } u;
} graph_value_t;

/* ============================================================
 * 存储限制常量
 * ============================================================ */

/** 最大标签数 */
#define GRAPH_MAX_LABELS 16

/** 最大属性数（需适配页面大小，约 1900 字节/顶点） */
#define GRAPH_MAX_PROPS 16

/** 最大属性名长度 */
#define GRAPH_MAX_PROP_NAME 32

/** 最大标签名长度 */
#define GRAPH_MAX_LABEL_NAME 64

/** 最大关系类型名长度 */
#define GRAPH_MAX_REL_TYPE_NAME 64

/* ============================================================
 * 对象状态
 * ============================================================ */

/** 顶点状态 */
typedef enum graph_vertex_state_e {
    GRAPH_VERTEX_LIVE = 0,       /**< 存活 */
    GRAPH_VERTEX_DELETED = 1     /**< 已删除 */
} graph_vertex_state_t;

/** 边状态 */
typedef enum graph_edge_state_e {
    GRAPH_EDGE_LIVE = 0,         /**< 存活 */
    GRAPH_EDGE_DELETED = 1       /**< 已删除 */
} graph_edge_state_t;

/* ============================================================
 * 顶点结构 (固定大小，用于 KV 存储)
 * ============================================================ */

/**
 * @brief 属性键值对（内联存储）
 */
typedef struct graph_prop_s {
    char        key[GRAPH_MAX_PROP_NAME];  /**< 属性名 */
    graph_value_type_t type;                /**< 类型 */
    graph_value_t  value;                   /**< 值 */
} graph_prop_t;

/**
 * @brief 顶点结构（紧凑设计，适配 8KB 页面）
 *
 * 布局: [固定头部][标签ID数组][属性数组]
 */
typedef struct graph_vertex_s {
    graph_vertex_id_t    id;                 /**< 顶点 ID（8 字节） */
    uint16_t            state;              /**< 状态 */
    uint16_t            n_labels;           /**< 标签数量 */
    uint16_t            n_props;            /**< 属性数量 */
    uint16_t            out_degree;         /**< 出度 */
    uint16_t            in_degree;          /**< 入度 */
    uint32_t            label_ids[GRAPH_MAX_LABELS];  /**< 标签 ID 数组 */
    graph_prop_t        props[GRAPH_MAX_PROPS];       /**< 内联属性 */
    /* 可变长属性（如长字符串）存储在溢出页 */
} graph_vertex_t;

/* ============================================================
 * 边结构 (固定大小，用于 KV 存储)
 * ============================================================ */

/**
 * @brief 边结构
 *
 * 布局: [固定头部][属性数组]
 */
typedef struct graph_edge_s {
    graph_edge_id_t     id;                 /**< 边 ID（8 字节） */
    graph_vertex_id_t   src_id;             /**< 源顶点 ID（8 字节） */
    graph_vertex_id_t   dst_id;             /**< 目标顶点 ID（8 字节） */
    uint32_t            rel_type_id;        /**< 关系类型 ID（4 字节） */
    uint16_t            n_props;            /**< 属性数量 */
    uint8_t             state;              /**< 状态 */
    uint8_t             direction;          /**< 方向: 0=双向, 1=正向, 2=反向 */
    graph_prop_t        props[GRAPH_MAX_PROPS];  /**< 内联属性 */
} graph_edge_t;

/* ============================================================
 * 路径结构
 * ============================================================ */

/**
 * @brief 路径结果
 */
typedef struct graph_path_s {
    graph_vertex_id_t *vertices;    /**< 路径顶点序列 */
    graph_edge_id_t   *edges;       /**< 路径边序列 */
    size_t            length;       /**< 跳数（边数） */
    double            weight;       /**< 总权重（用于最短路径） */
} graph_path_t;

/**
 * @brief 释放路径内存
 * @param path 路径
 */
void graph_path_free(graph_path_t *path);

/**
 * @brief 创建路径
 * @param length 路径长度
 * @return 路径，失败返回 NULL
 */
graph_path_t *graph_path_create(size_t length);

#ifdef __cplusplus
}
#endif

#endif /* DB_GRAPH_TYPES_H */
