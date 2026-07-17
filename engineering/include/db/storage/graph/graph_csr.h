/**
 * @file graph_csr.h
 * @brief 图 CSR (Compressed Sparse Row) 存储格式
 *
 * CSR 格式是一种高效的图存储方式，支持：
 * - O(1) 顶点出边查询
 * - O(1) 顶点入边查询（通过反向索引）
 * - 内存连续访问，缓存友好
 * - CSR + COO 混合模式支持增量更新
 */
#ifndef DB_GRAPH_CSR_H
#define DB_GRAPH_CSR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** CSR 文件魔数 */
#define GRAPH_CSR_MAGIC 0x47535231  /* "GSR1" */

/** CSR 文件版本 */
#define GRAPH_CSR_VERSION 1

/** 默认最大顶点数 */
#define GRAPH_CSR_DEFAULT_MAX_VERTICES 1000000

/** COO 缓冲区默认容量 */
#define GRAPH_CSR_COO_CAPACITY 10000

/** 最大标签数 */
#define GRAPH_CSR_MAX_LABELS 256

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/**
 * @brief 边结构
 */
typedef struct graph_csr_edge_s {
    uint64_t dst;           /**< 目标顶点 ID */
    uint32_t edge_type;     /**< 边类型 */
    uint64_t edge_id;       /**< 边 ID */
} graph_csr_edge_t;

/**
 * @brief 顶点结构
 */
typedef struct graph_csr_vertex_s {
    uint32_t label_id;      /**< 标签 ID */
    uint64_t prop_offset;   /**< 属性数据偏移 */
    uint32_t prop_size;     /**< 属性数据大小 */
} graph_csr_vertex_t;

/**
 * @brief 标签信息
 */
typedef struct graph_csr_label_s {
    char name[64];          /**< 标签名 */
    uint32_t vertex_count;  /**< 该标签的顶点数 */
} graph_csr_label_t;

/**
 * @brief COO 条目（增量缓冲区）
 */
typedef struct graph_csr_coo_entry_s {
    uint64_t src;           /**< 源顶点 */
    uint64_t dst;           /**< 目标顶点 */
    uint32_t edge_type;     /**< 边类型 */
    uint64_t edge_id;       /**< 边 ID */
    void *props;            /**< 边属性 */
    size_t prop_size;       /**< 属性大小 */
} graph_csr_coo_entry_t;

/**
 * @brief 标签索引项
 */
typedef struct graph_csr_label_index_entry_s {
    uint32_t label_id;
    uint32_t count;
    uint64_t *vertex_ids;   /**< 该标签的顶点 ID 数组 */
} graph_csr_label_index_entry_t;

/**
 * @brief 图 CSR 存储结构
 */
typedef struct graph_csr_s {
    char data_dir[512];     /**< 数据目录 */

    /* 顶点数据 */
    graph_csr_vertex_t *vertices;  /**< 顶点数组 */
    uint64_t vertex_count;         /**< 顶点数 */
    uint64_t max_vertices;         /**< 最大顶点数 */

    /* 边数据 (CSR 格式) */
    uint64_t *offsets;     /**< 偏移数组 [vertex_count + 1] */
    graph_csr_edge_t *edges;       /**< 边数组 */
    uint64_t edge_count;           /**< 边数 */

    /* 反向边索引 */
    uint64_t *in_offsets;          /**< 入边偏移数组 */
    graph_csr_edge_t *in_edges;    /**< 入边数组 */

    /* COO 增量缓冲区 */
    graph_csr_coo_entry_t *coo_entries;
    uint32_t coo_count;
    uint32_t coo_capacity;

    /* 标签管理 */
    graph_csr_label_t *labels;
    uint32_t label_count;

    /* 标签索引 */
    graph_csr_label_index_entry_t *label_index;
    uint32_t label_index_count;

    /* 统计信息 */
    uint64_t num_lookups;
    uint64_t num_edge_queries;
    double build_time_ms;
} graph_csr_t;

/**
 * @brief 边查询结果
 */
typedef struct graph_csr_edge_result_s {
    const graph_csr_edge_t *edges;
    uint32_t count;
} graph_csr_edge_result_t;

/* ========================================================================
 * 生命周期 API
 * ======================================================================== */

/**
 * @brief 创建 CSR 图存储
 *
 * @param data_dir 数据目录
 * @param max_vertices 最大顶点数
 * @return CSR 图句柄，失败返回 NULL
 */
graph_csr_t *graph_csr_create(const char *data_dir, uint64_t max_vertices);

/**
 * @brief 打开已存在的 CSR 图
 *
 * @param data_dir 数据目录
 * @return CSR 图句柄，失败返回 NULL
 */
graph_csr_t *graph_csr_open(const char *data_dir);

/**
 * @brief 保存 CSR 图到磁盘
 *
 * @param csr CSR 图句柄
 * @return 0 成功，-1 失败
 */
int graph_csr_save(graph_csr_t *csr);

/**
 * @brief 关闭并释放 CSR 图
 *
 * @param csr CSR 图句柄
 */
void graph_csr_destroy(graph_csr_t *csr);

/* ========================================================================
 * 顶点操作 API
 * ======================================================================== */

/**
 * @brief 添加顶点
 *
 * @param csr CSR 图句柄
 * @param label_id 标签 ID
 * @param props 属性数据（可选）
 * @param prop_size 属性大小
 * @return 顶点 ID，失败返回 UINT64_MAX
 */
uint64_t graph_csr_add_vertex(graph_csr_t *csr, uint32_t label_id,
                              const void *props, size_t prop_size);

/**
 * @brief 获取顶点信息
 *
 * @param csr CSR 图句柄
 * @param vertex_id 顶点 ID
 * @return 顶点指针，失败返回 NULL
 */
const graph_csr_vertex_t *graph_csr_get_vertex(const graph_csr_t *csr,
                                               uint64_t vertex_id);

/* ========================================================================
 * 边操作 API
 * ======================================================================== */

/**
 * @brief 添加边（增量模式，添加到 COO 缓冲区）
 *
 * @param csr CSR 图句柄
 * @param src 源顶点 ID
 * @param dst 目标顶点 ID
 * @param edge_type 边类型
 * @param props 边属性（可选）
 * @param prop_size 属性大小
 * @return 边 ID，失败返回 UINT64_MAX
 */
uint64_t graph_csr_add_edge(graph_csr_t *csr,
                            uint64_t src, uint64_t dst, uint32_t edge_type,
                            const void *props, size_t prop_size);

/**
 * @brief 获取顶点的出边
 *
 * O(1) 时间复杂度，直接通过 offsets 数组定位。
 *
 * @param csr CSR 图句柄
 * @param src 源顶点 ID
 * @param out_count 输出边数量
 * @return 出边数组（内部数据，勿释放）
 */
const graph_csr_edge_t *graph_csr_get_out_edges(const graph_csr_t *csr,
                                                uint64_t src, uint32_t *out_count);

/**
 * @brief 获取顶点的入边
 *
 * O(1) 时间复杂度，通过反向索引定位。
 *
 * @param csr CSR 图句柄
 * @param dst 目标顶点 ID
 * @param out_count 输出边数量
 * @return 入边数组（内部数据，勿释放）
 */
const graph_csr_edge_t *graph_csr_get_in_edges(const graph_csr_t *csr,
                                               uint64_t dst, uint32_t *out_count);

/* ========================================================================
 * COO 合并 API
 * ======================================================================== */

/**
 * @brief 合并 COO 缓冲区到 CSR
 *
 * 将增量添加的边合并到主 CSR 结构中，
 * 合并后清空 COO 缓冲区。
 *
 * @param csr CSR 图句柄
 * @return 0 成功，-1 失败
 */
int graph_csr_compact(graph_csr_t *csr);

/**
 * @brief 检查是否需要合并
 *
 * @param csr CSR 图句柄
 * @return true 需要合并
 */
bool graph_csr_needs_compact(const graph_csr_t *csr);

/**
 * @brief 获取 COO 缓冲区使用率
 *
 * @param csr CSR 图句柄
 * @return 使用率 (0.0 - 1.0)
 */
double graph_csr_coo_usage(const graph_csr_t *csr);

/* ========================================================================
 * 标签索引 API
 * ======================================================================== */

/**
 * @brief 获取或创建标签
 *
 * @param csr CSR 图句柄
 * @param label_name 标签名
 * @return 标签 ID，失败返回 UINT32_MAX
 */
uint32_t graph_csr_get_or_create_label(graph_csr_t *csr, const char *label_name);

/**
 * @brief 获取标签名
 *
 * @param csr CSR 图句柄
 * @param label_id 标签 ID
 * @return 标签名，失败返回 NULL
 */
const char *graph_csr_get_label_name(const graph_csr_t *csr, uint32_t label_id);

/**
 * @brief 按标签查询顶点
 *
 * @param csr CSR 图句柄
 * @param label_id 标签 ID
 * @param out_count 输出顶点数
 * @return 顶点 ID 数组（调用者负责释放）
 */
uint64_t *graph_csr_get_vertices_by_label(const graph_csr_t *csr,
                                          uint32_t label_id,
                                          uint32_t *out_count);

/**
 * @brief 构建标签索引
 *
 * @param csr CSR 图句柄
 */
void graph_csr_build_label_index(graph_csr_t *csr);

/* ========================================================================
 * 统计信息 API
 * ======================================================================== */

/**
 * @brief 获取 CSR 图统计信息
 *
 * @param csr CSR 图句柄
 * @param out_vertices 输出顶点数
 * @param out_edges 输出边数
 * @param out_labels 输出标签数
 */
void graph_csr_get_stats(const graph_csr_t *csr,
                         uint64_t *out_vertices, uint64_t *out_edges,
                         uint32_t *out_labels);

#ifdef __cplusplus
}
#endif

#endif /* DB_GRAPH_CSR_H */
