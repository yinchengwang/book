/**
 * @file vector_types.h
 * @brief 向量查询执行器基础类型定义
 *
 * 独立于 SQL 执行器的类型定义，避免循环依赖。
 */
#ifndef DB_VECTOR_TYPES_H
#define DB_VECTOR_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <algo-prod/distance/distance.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 距离度量类型别名 (向后兼容)
 * ======================================================================== */

/** 距离度量类型 (兼容 distance_metric_t) */
typedef distance_metric_t DistanceMetric;

/* ========================================================================
 * 查询执行器状态
 * ======================================================================== */

/** 查询执行器状态 */
typedef enum VectorQueryState_e {
    VECTOR_QUERY_UNINIT = 0,  /**< 未初始化 */
    VECTOR_QUERY_READY = 1,   /**< 就绪 */
    VECTOR_QUERY_ERROR = 2    /**< 错误状态 */
} VectorQueryState;

/** 执行阶段 */
typedef enum VectorQueryStage_e {
    STAGE_PLAN = 0,       /**< 查询计划 */
    STAGE_COARSE = 1,     /**< 粗排 */
    STAGE_RERANK = 2,     /**< 精排 */
    STAGE_FUSION = 3,     /**< 融合 */
    STAGE_OUTPUT = 4      /**< 输出 */
} VectorQueryStage;

/** 剖析条目 */
typedef struct VectorQueryProfileEntry_s {
    VectorQueryStage stage;       /**< 执行阶段 */
    int64_t start_us;             /**< 开始时间 */
    int64_t end_us;               /**< 结束时间 */
    int64_t duration_us;          /**< 持续时间 */
    int32_t items_processed;      /**< 处理项数 */
} VectorQueryProfileEntry;

/* ========================================================================
 * 列块数据结构
 * ======================================================================== */

/** 向量化执行模式 */
typedef enum VectorExecMode_e {
    VECTOR_EXEC_BATCH = 0,    /**< 批量处理模式 */
    VECTOR_EXEC_ITER,         /**< 迭代器模式 */
    VECTOR_EXEC_HYBRID        /**< 混合模式 */
} VectorExecMode;

/** 列块 */
typedef struct VectorBlock_s {
    int num_rows;             /**< 行数 */
    int capacity;             /**< 容量 */
    void **columns;           /**< 列数据数组 */
    int *column_sizes;        /**< 每列大小 */
    int num_columns;          /**< 列数 */
    uint64_t *null_bitmap;    /**< 空值位图 */
    uint64_t *sel_bitmap;     /**< 选择位图 */
    int *selection_vector;    /**< 选择向量 */
} VectorBlock;

/** 批次 */
typedef struct VectorBatch_s {
    VectorBlock **blocks;     /**< 列块数组 */
    int num_blocks;           /**< 列块数量 */
    int num_rows;             /**< 总行数 */
    int capacity;             /**< 容量 */
    int current_block;        /**< 当前列块索引 */
} VectorBatch;

/* ========================================================================
 * 比较操作符
 * ======================================================================== */

typedef enum CompareOp_e {
    CMP_EQ = 0,              /**< 等于 */
    CMP_NE,                  /**< 不等于 */
    CMP_LT,                  /**< 小于 */
    CMP_LE,                  /**< 小于等于 */
    CMP_GT,                  /**< 大于 */
    CMP_GE                   /**< 大于等于 */
} CompareOp;

/** 过滤结果 */
typedef struct VectorFilterResult_s {
    int64_t *matches;        /**< 匹配的行索引 */
    int num_matches;         /**< 匹配数量 */
} VectorFilterResult;

/* ========================================================================
 * 列块操作 API
 * ======================================================================== */

VectorBlock *vector_block_create(int capacity, int num_columns);
void vector_block_destroy(VectorBlock *block);
int vector_block_set_column(VectorBlock *block, int col_idx, void *data, int element_size);
void *vector_block_get_column(VectorBlock *block, int col_idx);
void vector_block_set_num_rows(VectorBlock *block, int num_rows);
void vector_block_set_null(VectorBlock *block, int row_idx, bool isnull);
bool vector_block_is_null(VectorBlock *block, int row_idx);

/* ========================================================================
 * 批次操作 API
 * ======================================================================== */

VectorBatch *vector_batch_create(int capacity);
void vector_batch_destroy(VectorBatch *batch);
int vector_batch_add_block(VectorBatch *batch, VectorBlock *block);
VectorBlock *vector_batch_next(VectorBatch *batch);

/* ========================================================================
 * SIMD 过滤 API
 * ======================================================================== */

void vector_filter_int_simd(const int32_t *a, int32_t b, int num_elements, CompareOp op, uint64_t *result);
void vector_filter_float_simd(const float *a, float b, int num_elements, CompareOp op, uint64_t *result);
void vector_filter_string_simd(const char **a, const char *b, int num_elements, CompareOp op, uint64_t *result);
VectorFilterResult *vector_filter_execute(VectorBlock *block, int column_idx, void *value, CompareOp op);
void vector_filter_result_free(VectorFilterResult *result);

#ifdef __cplusplus
}
#endif

#endif /* DB_VECTOR_TYPES_H */
