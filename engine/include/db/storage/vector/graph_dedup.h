/**
 * @file graph_dedup.h
 * @brief 图索引去重模块
 *
 * 用于图索引层去重，支持：
 * - SimHash 快速检测
 * - Bloom Filter 预检
 * - 反向映射（graph_node_id → heap_row_ids）
 */
#ifndef GRAPH_DEDUP_H
#define GRAPH_DEDUP_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 默认假阳性率 */
#define GRAPH_DEDUP_DEFAULT_FP_RATE 0.05f

/** 最小重复检测距离阈值 */
#define GRAPH_DEDUP_DEFAULT_THRESHOLD 0.001f

/** 默认哈希表容量 */
#define GRAPH_DEDUP_DEFAULT_CAPACITY 100000

/** 反向映射初始容量 */
#define GRAPH_DEDUP_REVERSE_INIT_CAPACITY 16

/** 文件魔数 */
#define GRAPH_DEDUP_FILE_MAGIC 0x47444544  /* "GDED" */

/** 文件版本 */
#define GRAPH_DEDUP_FILE_VERSION 1

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/**
 * @brief SimHash 指纹
 */
typedef struct dedup_fingerprint_s {
    uint64_t hash;          /**< 64 位 SimHash */
    float norm;             /**< 向量范数 */
} dedup_fingerprint_t;

/**
 * @brief 反向映射条目
 */
typedef struct dedup_reverse_entry_s {
    int32_t graph_node_id;      /**< 图节点 ID */
    int32_t heap_row_count;     /**< 关联的 Heap 行数 */
    int32_t capacity;           /**< 容量 */
    int32_t *heap_row_ids;      /**< Heap 行 ID 数组 */
} dedup_reverse_entry_t;

/**
 * @brief 去重统计
 */
typedef struct dedup_stats_s {
    uint64_t total_checks;      /**< 总检查次数 */
    uint64_t duplicates_found;  /**< 发现重复次数 */
    uint64_t unique_inserted;    /**< 唯一向量插入次数 */
    uint64_t bloom_positive;     /**< Bloom Filter 阳性次数 */
} dedup_stats_t;

/**
 * @brief 图去重器
 */
typedef struct graph_dedup_s {
    /* 配置 */
    int32_t dims;                   /**< 向量维度 */
    float threshold;                /**< 重复检测阈值（距离） */

    /* SimHash 存储 */
    dedup_fingerprint_t *fingerprints;  /**< SimHash 数组 */
    int32_t count;                      /**< 已存储向量数 */
    int32_t capacity;                   /**< 容量 */

    /* 精确比较缓冲区（用于二次确认） */
    float *exact_vectors;            /**< 精确向量存储 */
    int32_t exact_capacity;           /**< 精确存储容量 */

    /* Bloom Filter 简化实现 */
    uint64_t *bloom_bitset;           /**< 位图 */
    int32_t bloom_size;               /**< 位图大小（位数） */

    /* 反向映射 */
    dedup_reverse_entry_t *reverse_map;  /**< 反向映射表 */
    int32_t reverse_count;               /**< 反向映射条目数 */
    int32_t reverse_capacity;            /**< 反向映射容量 */

    /* 统计 */
    dedup_stats_t stats;
} graph_dedup_t;

/* ========================================================================
 * 生命周期 API
 * ======================================================================== */

/**
 * @brief 创建图去重器
 *
 * @param capacity 预估容量
 * @param dims 向量维度
 * @param fp_rate 假阳性率（0.01-0.1）
 * @param threshold 重复检测阈值（距离）
 * @return 去重器指针
 */
graph_dedup_t *graph_dedup_create(int32_t capacity,
                                  int32_t dims,
                                  float fp_rate,
                                  float threshold);

/**
 * @brief 销毁图去重器
 *
 * @param dedup 去重器
 */
void graph_dedup_destroy(graph_dedup_t *dedup);

/**
 * @brief 重置去重器状态
 *
 * @param dedup 去重器
 */
void graph_dedup_reset(graph_dedup_t *dedup);

/* ========================================================================
 * 去重检测 API
 * ======================================================================== */

/**
 * @brief 计算向量 SimHash
 *
 * @param dedup 去重器
 * @param vector 向量
 * @param out_fingerprint 输出指纹
 */
void graph_dedup_compute_fingerprint(graph_dedup_t *dedup,
                                    const float *vector,
                                    dedup_fingerprint_t *out_fingerprint);

/**
 * @brief 检测向量是否重复
 *
 * @param dedup 去重器
 * @param vector 向量
 * @return true 重复，false 不重复
 */
bool graph_dedup_is_duplicate(graph_dedup_t *dedup, const float *vector);

/**
 * @brief 标记向量已插入索引
 *
 * @param dedup 去重器
 * @param vector 向量
 * @param graph_node_id 图节点 ID
 * @param heap_row_id Heap 行 ID
 * @return 0 成功，-1 失败
 */
int graph_dedup_mark_inserted(graph_dedup_t *dedup,
                               const float *vector,
                               int32_t graph_node_id,
                               int32_t heap_row_id);

/**
 * @brief 批量标记向量已插入
 *
 * @param dedup 去重器
 * @param vectors 向量数组
 * @param graph_node_ids 图节点 ID 数组
 * @param heap_row_ids Heap 行 ID 数组
 * @param count 数量
 * @return 成功数量
 */
int graph_dedup_mark_inserted_batch(graph_dedup_t *dedup,
                                     const float *vectors,
                                     const int32_t *graph_node_ids,
                                     const int32_t *heap_row_ids,
                                     int32_t count);

/* ========================================================================
 * 反向映射查询 API
 * ======================================================================== */

/**
 * @brief 获取图节点关联的所有 Heap 行
 *
 * @param dedup 去重器
 * @param graph_node_id 图节点 ID
 * @param out_rows 输出数组
 * @param max_rows 最大数量
 * @return 实际数量
 */
int graph_dedup_get_heap_rows(const graph_dedup_t *dedup,
                              int32_t graph_node_id,
                              int32_t *out_rows,
                              int32_t max_rows);

/**
 * @brief 获取图节点关联的第一个 Heap 行
 *
 * @param dedup 去重器
 * @param graph_node_id 图节点 ID
 * @return Heap 行 ID，-1 表示未找到
 */
int32_t graph_dedup_get_first_heap_row(const graph_dedup_t *dedup,
                                        int32_t graph_node_id);

/**
 * @brief 获取图节点关联的 Heap 行数
 *
 * @param dedup 去重器
 * @param graph_node_id 图节点 ID
 * @return 行数
 */
int graph_dedup_get_heap_row_count(const graph_dedup_t *dedup,
                                    int32_t graph_node_id);

/* ========================================================================
 * 持久化 API
 * ======================================================================== */

/**
 * @brief 保存去重器状态到文件
 *
 * @param dedup 去重器
 * @param path 文件路径
 * @return 0 成功，-1 失败
 */
int graph_dedup_save(const graph_dedup_t *dedup, const char *path);

/**
 * @brief 从文件加载去重器状态
 *
 * @param path 文件路径
 * @return 去重器指针，失败返回 NULL
 */
graph_dedup_t *graph_dedup_load(const char *path);

/* ========================================================================
 * 统计 API
 * ======================================================================== */

/**
 * @brief 获取去重统计
 *
 * @param dedup 去重器
 * @param stats 输出统计
 */
void graph_dedup_get_stats(const graph_dedup_t *dedup, dedup_stats_t *stats);

/**
 * @brief 重置统计
 *
 * @param dedup 去重器
 */
void graph_dedup_reset_stats(graph_dedup_t *dedup);

/**
 * @brief 获取去重率
 *
 * @param dedup 去重器
 * @return 去重百分比
 */
float graph_dedup_get_dedup_rate(const graph_dedup_t *dedup);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 计算向量范数
 *
 * @param vector 向量
 * @param dim 维度
 * @return 范数
 */
float graph_dedup_compute_norm(const float *vector, int32_t dim);

/**
 * @brief 计算两个向量的 L2 距离
 *
 * @param a 向量 A
 * @param b 向量 B
 * @param dim 维度
 * @return L2 距离
 */
float graph_dedup_l2_distance(const float *a, const float *b, int32_t dim);

/**
 * @brief 计算向量与已存储向量的最小距离
 *
 * @param dedup 去重器
 * @param vector 向量
 * @param out_min_dist 输出最小距离
 * @return 匹配的向量索引，-1 表示无匹配
 */
int32_t graph_dedup_find_min_distance(const graph_dedup_t *dedup,
                                       const float *vector,
                                       float *out_min_dist);

/**
 * @brief 调整去重器容量
 *
 * @param dedup 去重器
 * @param new_capacity 新容量
 * @return 0 成功，-1 失败
 */
int graph_dedup_reserve(graph_dedup_t *dedup, int32_t new_capacity);

#ifdef __cplusplus
}
#endif

#endif /* GRAPH_DEDUP_H */
