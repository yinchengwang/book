/**
 * @file index.h
 * @brief 图索引接口
 *
 * 支持 Label 索引和属性索引
 */
#ifndef DB_GRAPH_INDEX_H
#define DB_GRAPH_INDEX_H

#include "types.h"
#include "graph.h"
#include <stdint.h>

/* 定义 Oid 类型（如果未定义） */
#ifndef Oid
typedef uint32_t Oid;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 索引类型
 * ============================================================ */

/** 图索引类型 */
typedef enum graph_index_type_e {
    GRAPH_INDEX_LABEL = 0,       /**< Label 索引：按标签快速查找顶点 */
    GRAPH_INDEX_PROPERTY = 1,    /**< 属性索引：按属性值查找 */
    GRAPH_INDEX_EXISTENCE = 2    /**< 存在性索引：检查属性是否存在 */
} graph_index_type_t;

/** 索引信息 */
typedef struct graph_index_info_s {
    Oid                 oid;             /**< 索引 OID */
    char                name[64];        /**< 索引名 */
    graph_index_type_t  type;            /**< 索引类型 */
    char                label[64];       /**< 所属标签 */
    char                property[64];    /**< 索引的属性 */
    bool                is_unique;       /**< 是否唯一 */
    bool                is_valid;        /**< 是否有效 */
} graph_index_info_t;

/* ============================================================
 * 索引操作
 * ============================================================ */

/**
 * @brief 创建索引
 * @param g 图数据库
 * @param name 索引名
 * @param type 索引类型
 * @param label 标签名（用于 LABEL 和 PROPERTY 索引）
 * @param property 属性名（用于 PROPERTY 索引，可为 NULL）
 * @param is_unique 是否唯一
 * @return 索引 OID，失败返回 0
 */
Oid graph_index_create(graph_t *g,
                       const char *name,
                       graph_index_type_t type,
                       const char *label,
                       const char *property,
                       bool is_unique);

/**
 * @brief 删除索引
 * @param g 图数据库
 * @param index_oid 索引 OID
 * @return 0 成功，-1 失败
 */
int graph_index_drop(graph_t *g, Oid index_oid);

/**
 * @brief 列出所有索引
 * @param g 图数据库
 * @param out_count 输出索引数量
 * @return 索引信息数组（调用者负责释放）
 */
graph_index_info_t *graph_list_indexes(graph_t *g, size_t *out_count);

/**
 * @brief 释放索引信息数组
 * @param indexes 索引信息数组
 */
void graph_index_info_free(graph_index_info_t *indexes);

/* ============================================================
 * 索引扫描
 * ============================================================ */

/** 索引扫描句柄 */
typedef struct graph_index_scan_s graph_index_scan_t;

/**
 * @brief 创建等值扫描
 * @param g 图数据库
 * @param label 标签名
 * @param property 属性名
 * @param value 值
 * @return 扫描句柄
 */
graph_index_scan_t *graph_index_scan_eq(graph_t *g,
                                        const char *label,
                                        const char *property,
                                        const graph_value_t *value);

/**
 * @brief 创建范围扫描
 * @param g 图数据库
 * @param label 标签名
 * @param property 属性名
 * @param min 最小值（可为 NULL）
 * @param max 最大值（可为 NULL）
 * @return 扫描句柄
 */
graph_index_scan_t *graph_index_scan_range(graph_t *g,
                                           const char *label,
                                           const char *property,
                                           const graph_value_t *min,
                                           const graph_value_t *max);

/**
 * @brief 获取下一个匹配顶点
 * @param scan 扫描句柄
 * @param out_vid 输出顶点 ID
 * @return true 有更多结果，false 扫描完成
 */
bool graph_index_scan_next(graph_index_scan_t *scan, graph_vertex_id_t *out_vid);

/**
 * @brief 关闭扫描
 * @param scan 扫描句柄
 */
void graph_index_scan_close(graph_index_scan_t *scan);

#ifdef __cplusplus
}
#endif

#endif /* DB_GRAPH_INDEX_H */
