/**
 * @file vector_exec.h
 * @brief 向量化执行引擎头文件
 *
 * 实现列块数据结构、SIMD 距离计算、SIMD 过滤、向量化 Scan/HashJoin/Aggregation
 */
#ifndef DB_VECTOR_EXEC_H
#define DB_VECTOR_EXEC_H

#include <db/core/vector_types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 列块数据结构 (已在 vector_types.h 中定义)
 * ======================================================================== */

/* VectorExecMode, VectorBlock, VectorBatch 已在 vector_types.h 定义 */

/**
 * @brief 创建列块
 * @param capacity 容量
 * @param num_columns 列数
 */
VectorBlock *vector_block_create(int capacity, int num_columns);

/**
 * @brief 销毁列块
 */
void vector_block_destroy(VectorBlock *block);

/**
 * @brief 设置列块数据
 */
int vector_block_set_column(VectorBlock *block, int col_idx,
                           void *data, int element_size);

/**
 * @brief 获取列数据
 */
void *vector_block_get_column(VectorBlock *block, int col_idx);

/**
 * @brief 设置行数
 */
void vector_block_set_num_rows(VectorBlock *block, int num_rows);

/**
 * @brief 设置 NULL 位图
 */
void vector_block_set_null(VectorBlock *block, int row_idx, bool isnull);

/**
 * @brief 检查 NULL
 */
bool vector_block_is_null(VectorBlock *block, int row_idx);

/**
 * @brief 创建批次
 */
VectorBatch *vector_batch_create(int capacity);

/**
 * @brief 销毁批次
 */
void vector_batch_destroy(VectorBatch *batch);

/**
 * @brief 添加块到批次
 */
int vector_batch_add_block(VectorBatch *batch, VectorBlock *block);

/**
 * @brief 获取下一块
 */
VectorBlock *vector_batch_next(VectorBatch *batch);

/* ========================================================================
 * SIMD 距离计算
 * ======================================================================== */

/* DistanceMetric 已在 vector_types.h 中定义（别名为 DistanceMetric） */

/**
 * @brief SIMD 加速的 L2 距离计算
 * @param a 向量 A
 * @param b 向量 B
 * @param dim 维度
 * @return 距离
 */
float vector_distance_l2_simd(const float *a, const float *b, int dim);

/**
 * @brief SIMD 加速的余弦距离计算
 */
float vector_distance_cosine_simd(const float *a, const float *b, int dim);

/**
 * @brief SIMD 加速的点积计算
 */
float vector_dot_product_simd(const float *a, const float *b, int dim);

/**
 * @brief 批量计算 L2 距离
 * @param query 查询向量
 * @param vectors 向量数组
 * @param dim 维度
 * @param num_vectors 向量数量
 * @param distances 输出距离数组
 */
void vector_batch_l2_distance_simd(const float *query,
                                 const float **vectors,
                                 int dim,
                                 int num_vectors,
                                 float *distances);

/**
 * @brief 批量计算余弦距离
 */
void vector_batch_cosine_distance_simd(const float *query,
                                      const float **vectors,
                                      int dim,
                                      int num_vectors,
                                      float *distances);

/**
 * @brief 检测是否支持 SIMD
 */
bool vector_has_simd_support(void);

/**
 * @brief 获取 SIMD 扩展类型
 */
const char *vector_get_simd_type(void);

/* ========================================================================
 * SIMD 过滤 (CompareOp 和 VectorFilterResult 已在 vector_types.h 中定义)
 * ======================================================================== */

/* CompareOp 和 VectorFilterResult 已在 vector_types.h 定义 */

/**
 * @brief SIMD 整数比较
 * @param a 输入 A
 * @param b 输入 B
 * @param num_elements 元素数
 * @param op 比较操作符
 * @param result 输出位图
 */
void vector_filter_int_simd(const int32_t *a, int32_t b,
                           int num_elements,
                           CompareOp op,
                           uint64_t *result);

/**
 * @brief SIMD 浮点数比较
 */
void vector_filter_float_simd(const float *a, float b,
                            int num_elements,
                            CompareOp op,
                            uint64_t *result);

/**
 * @brief SIMD 字符串比较
 */
void vector_filter_string_simd(const char **a, const char *b,
                             int num_elements,
                             CompareOp op,
                             uint64_t *result);

/**
 * @brief 执行 SIMD 过滤
 */
VectorFilterResult *vector_filter_execute(VectorBlock *block,
                                         int column_idx,
                                         void *value,
                                         CompareOp op);

/**
 * @brief 释放过滤结果
 */
void vector_filter_result_free(VectorFilterResult *result);

/**
 * @brief 组合多个过滤条件
 * @param results 结果数组
 * @param num_results 结果数量
 * @param mode AND 或 OR
 */
void vector_filter_combine(VectorFilterResult **results,
                         size_t num_results,
                         bool is_and);

/* ========================================================================
 * 向量化算子
 * ======================================================================== */

/* 注意：VectorScanState/VectorHashJoinState/VectorAggState 需要包含 sql_executor.h
   才能使用完整的 PlanState/JoinState 类型。当前使用指针避免直接依赖。 */

struct PlanState_s;
struct JoinState_s;
struct TupleTableSlot_s;
struct Expr_s;

/* 使用不同的结构名避免与 sql_executor.h 中的 VectorScanState 冲突 */
typedef struct VectorScanExecState_s {
    struct PlanState_s *ps;   /**< 计划状态 (避免直接依赖) */
    VectorExecMode mode;     /**< 执行模式 */
    int batch_size;           /**< 批次大小 */
    VectorBatch *batch;       /**< 当前批次 */
    int current_row;         /**< 当前行 */
    struct Expr_s *filter_expr; /**< 过滤表达式 */
} VectorScanExecState;

typedef struct VectorHashJoinExecState_s {
    struct JoinState_s *js;   /**< 连接状态 (避免直接依赖) */
    VectorExecMode mode;     /**< 执行模式 */
    int batch_size;           /**< 批次大小 */
    void *hash_table;        /**< Hash 表 */
    VectorBatch *build_batch; /**< 构建批次 */
    VectorBatch *probe_batch; /**< 探测批次 */
    int current_build_row;    /**< 当前构建行 */
    int current_probe_row;    /**< 当前探测行 */
} VectorHashJoinExecState;

typedef struct VectorAggExecState_s {
    struct PlanState_s *ps;   /**< 计划状态 (避免直接依赖) */
    VectorExecMode mode;     /**< 执行模式 */
    int batch_size;           /**< 批次大小 */
    void *hash_table;        /**< Hash 表 */
    void *group_vectors;     /**< 分组向量 */
    int num_groups;           /**< 组数 */
    int num_aggs;             /**< 聚合函数数 */
} VectorAggExecState;

/**
 * @brief 创建向量化 Scan 状态
 */
VectorScanExecState *vector_scan_state_create(int batch_size);

/**
 * @brief 执行向量化 Scan
 */
struct TupleTableSlot_s *vector_scan_exec(VectorScanExecState *state);

/**
 * @brief 创建向量化 HashJoin 状态
 */
VectorHashJoinExecState *vector_hashjoin_state_create(int batch_size);

/**
 * @brief 执行向量化 HashJoin
 */
struct TupleTableSlot_s *vector_hashjoin_exec(VectorHashJoinExecState *state);

/**
 * @brief 创建向量化聚合状态
 */
VectorAggExecState *vector_agg_state_create(int batch_size, int num_groups);

/**
 * @brief 执行向量化聚合
 */
struct TupleTableSlot_s *vector_agg_exec(VectorAggExecState *state);

/**
 * @brief 向量化过滤
 */
int vector_filter(VectorScanExecState *state, struct Expr_s *filter_expr);

/* ========================================================================
 * SIMD 指令检测
 * ======================================================================== */

/** SIMD 扩展类型 */
typedef enum SimdExtension_e {
    SIMD_NONE = 0,           /**< 无 SIMD */
    SIMD_SSE,                /**< SSE */
    SIMD_SSE2,               /**< SSE2 */
    SIMD_SSE4,               /**< SSE4 */
    SIMD_AVX,                /**< AVX */
    SIMD_AVX2,               /**< AVX2 */
    SIMD_AVX512,             /**< AVX-512 */
    SIMD_NEON                 /**< ARM NEON */
} SimdExtension;

/**
 * @brief 检测 CPU 支持的 SIMD 扩展
 */
SimdExtension simd_detect_extension(void);

/**
 * @brief 检查是否支持指定扩展
 */
bool simd_has_extension(SimdExtension ext);

/**
 * @brief 获取最优 SIMD 扩展
 */
SimdExtension simd_get_best_extension(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_VECTOR_EXEC_H */
