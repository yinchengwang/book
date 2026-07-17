/**
 * @file cost.h
 * @brief Selinger 代价模型接口
 *
 * 实现 PostgreSQL 风格的代价估算模型：
 * 1. 代价参数（CostParams）
 * 2. 统计信息（RelStats、AttStats）
 * 3. 代价计算函数（cost_seqscan、cost_hashjoin 等）
 * 4. 选择率估算（estimate_selectivity）
 * 5. 基数估算（estimate_num_groups）
 */
#ifndef DB_SQL_COST_H
#define DB_SQL_COST_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 类型定义
 * ======================================================================== */

typedef double Cost;            /**< 代价类型 */
typedef uint64_t Oid;           /**< 对象标识符（与 execnodes.h 一致） */

/* ========================================================================
 * 代价参数
 * ======================================================================== */

/**
 * @brief 代价常量参数
 *
 * 这些参数来自 PostgreSQL GUC 配置，控制代价计算的权重。
 */
typedef struct CostParams {
    double  cpu_tuple_cost;         /**< 处理一行的 CPU 代价 */
    double  cpu_index_tuple_cost;   /**< 索引扫描一行代价 */
    double  cpu_operator_cost;      /**< 执行一个操作符的代价 */
    double  random_page_cost;       /**< 随机 I/O 代价 */
    double  seq_page_cost;          /**< 顺序 I/O 代价 */
    double  parallel_tuple_cost;    /**< 并行传递一行代价 */
    double  parallel_setup_cost;    /**< 并行启动代价 */
} CostParams;

/* 默认代价参数值 */
#define DEFAULT_CPU_TUPLE_COST       0.01
#define DEFAULT_CPU_INDEX_TUPLE_COST 0.005
#define DEFAULT_CPU_OPERATOR_COST    0.0025
#define DEFAULT_RANDOM_PAGE_COST     4.0
#define DEFAULT_SEQ_PAGE_COST        1.0
#define DEFAULT_PARALLEL_TUPLE_COST  0.1
#define DEFAULT_PARALLEL_SETUP_COST  1000.0

/* ========================================================================
 * 统计信息
 * ======================================================================== */

/**
 * @brief 表统计信息
 *
 * 来自 pg_class 系统表，用于代价估算。
 */
typedef struct RelStats {
    Oid         relid;              /**< 表 OID */
    double      relpages;           /**< 页面数 */
    double      reltuples;          /**< 行数 */
    double      relallvisible;      /**< 全可见页面数 */
} RelStats;

/**
 * @brief 列统计信息
 *
 * 来自 pg_statistics 系统表，用于选择率估算。
 */
typedef struct AttStats {
    Oid         attrelid;           /**< 所属表 OID */
    int         attnum;             /**< 属性编号 */
    double      ndistinct;          /**< 不同值数量 */
    double      nullfrac;           /**< NULL 比例 */
    double      avg_width;          /**< 平均宽度（字节） */
} AttStats;

/* ========================================================================
 * Plan 前向声明
 * ======================================================================== */

struct Plan;
struct Expr;

/* ========================================================================
 * 代价计算 API
 * ======================================================================== */

/**
 * @brief 获取默认代价参数
 *
 * @return 默认代价参数结构
 */
CostParams get_default_cost_params(void);

/**
 * @brief 计算顺序扫描代价（内部函数）
 *
 * 代价公式：seq_page_cost * relpages + cpu_tuple_cost * reltuples
 *
 * @param relpages  页面数
 * @param reltuples 行数
 *
 * @return 顺序扫描代价
 */
Cost compute_seqscan_cost(double relpages, double reltuples);

/**
 * @brief 计算索引扫描代价（内部函数）
 *
 * 代价公式：random_page_cost * pages + cpu_index_tuple_cost * tuples
 *
 * @param relpages  页面数
 * @param reltuples 行数
 * @param selectivity 选择率
 *
 * @return 索引扫描代价
 */
Cost compute_indexscan_cost(double relpages, double reltuples, double selectivity);

/**
 * @brief 计算 HashJoin 代价（内部函数）
 *
 * 代价公式：build_cost + probe_cost
 * - build_cost = cpu_tuple_cost * inner_rows + cpu_operator_cost * inner_rows（哈希计算）
 * - probe_cost = cpu_tuple_cost * outer_rows + cpu_operator_cost * outer_rows（哈希查找）
 *
 * @param outer_rows 外表行数
 * @param inner_rows 内表行数
 *
 * @return HashJoin 代价
 */
Cost compute_hashjoin_cost(double outer_rows, double inner_rows);

/**
 * @brief 计算聚合代价（内部函数）
 *
 * 代价公式：cpu_tuple_cost * num_groups + cpu_operator_cost * num_groups
 *
 * @param num_groups 预估分组数
 *
 * @return 聚合代价
 */
Cost compute_agg_cost(double num_groups);

/**
 * @brief 计算排序代价（内部函数）
 *
 * 代价公式：cpu_operator_cost * N * log2(N) + cpu_tuple_cost * N
 *
 * @param num_tuples  待排序行数
 *
 * @return 排序代价
 */
Cost compute_sort_cost(double num_tuples);

/* ========================================================================
 * 选择率和基数估算
 * ======================================================================== */

/**
 * @brief 估算选择率
 *
 * 根据统计信息估算谓词的选择率。
 * - 等值条件：1 / ndistinct
 * - 范围条件：(high - value) / (high - low)
 * - 默认：0.5
 *
 * @param stats  列统计信息
 * @param clause 谓词表达式
 *
 * @return 选择率（0.0 ~ 1.0）
 */
double estimate_selectivity(AttStats *stats, struct Expr *clause);

/**
 * @brief 估算分组数
 *
 * 根据统计信息估算 GROUP BY 的分组数。
 * 公式：min(reltuples, Π ndistinct)
 *
 * @param stats      表统计信息
 * @param group_keys 分组键统计信息数组
 * @param nkeys      分组键数量
 *
 * @return 预估分组数
 */
double estimate_num_groups(RelStats *stats, AttStats **group_keys, int nkeys);

#ifdef __cplusplus
}
#endif

#endif  // DB_SQL_COST_H
