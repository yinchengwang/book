/**
 * @file optimizer.h
 * @brief 查询优化规则接口
 *
 * 实现 8 条核心查询优化规则：
 * 1. 谓词下推（Predicate Pushdown）
 * 2. 投影下推（Projection Pushdown）
 * 3. 连接顺序优化（Join Order Optimization）
 * 4. 常量折叠（Constant Folding）
 * 5. 子查询展开（Subquery Unnesting）
 * 6. 聚合下推（Aggregation Pushdown）
 * 7. 排序优化（Sort Elimination）
 * 8. 限制下推（Limit Pushdown）
 */
#ifndef DB_SQL_OPTIMIZER_H
#define DB_SQL_OPTIMIZER_H

#include <stdbool.h>
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

/* Plan 已在 execnodes.h 中定义 */

/* ========================================================================
 * 优化规则类型
 * ======================================================================== */

/**
 * @brief 优化规则类型枚举
 *
 * 定义支持的优化规则类型，用于规则选择和应用。
 */
typedef enum OptRule {
    OPT_RULE_PREDICATE_PUSHDOWN = 0,    /**< 谓词下推：将 WHERE 条件推到扫描节点 */
    OPT_RULE_PROJECTION_PUSHDOWN,       /**< 投影下推：将 SELECT 列表推到扫描节点 */
    OPT_RULE_JOIN_ORDER,                /**< 连接顺序优化：基于代价选择连接顺序 */
    OPT_RULE_CONSTANT_FOLDING,          /**< 常量折叠：编译期计算常量表达式 */
    OPT_RULE_SUBQUERY_UNNEST,           /**< 子查询展开：将相关子查询转换为连接 */
    OPT_RULE_AGG_PUSHDOWN,              /**< 聚合下推：推送聚合到扫描节点 */
    OPT_RULE_SORT_ELIMINATION,          /**< 排序优化：避免不必要的排序 */
    OPT_RULE_LIMIT_PUSHDOWN,            /**< 限制下推：推送 LIMIT 到扫描节点 */
    OPT_RULE_MAX                        /**< 规则数量上限 */
} OptRule;

/* ========================================================================
 * 优化器配置
 * ======================================================================== */

/**
 * @brief 优化器配置结构
 *
 * 控制各优化规则的启用/禁用状态。
 */
typedef struct OptimizerConfig {
    bool    enable_predicate_pushdown;      /**< 启用谓词下推 */
    bool    enable_projection_pushdown;     /**< 启用投影下推 */
    bool    enable_join_order;              /**< 启用连接顺序优化 */
    bool    enable_constant_folding;        /**< 启用常量折叠 */
    bool    enable_subquery_unnest;         /**< 启用子查询展开 */
    bool    enable_agg_pushdown;            /**< 启用聚合下推 */
    bool    enable_sort_elimination;        /**< 启用排序优化 */
    bool    enable_limit_pushdown;          /**< 启用限制下推 */
} OptimizerConfig;

/* ========================================================================
 * 核心 API
 * ======================================================================== */

/**
 * @brief 获取默认优化器配置
 *
 * 默认启用所有优化规则。
 *
 * @return 默认配置结构
 */
OptimizerConfig get_default_optimizer_config(void);

/**
 * @brief 优化查询计划
 *
 * 根据配置应用所有启用的优化规则。
 *
 * @param plan   待优化的计划树
 * @param config 优化器配置（NULL 使用默认配置）
 *
 * @return 优化后的计划树
 */
Plan *optimize_plan(Plan *plan, OptimizerConfig *config);

/**
 * @brief 应用单条优化规则
 *
 * @param plan 待优化的计划树
 * @param rule 要应用的规则
 *
 * @return 优化后的计划树
 */
Plan *apply_opt_rule(Plan *plan, OptRule rule);

/* ========================================================================
 * 各优化规则实现
 * ======================================================================== */

/**
 * @brief 谓词下推
 *
 * 将 WHERE 条件尽可能下推到扫描节点，减少上层处理的行数。
 *
 * 示例：
 *   SELECT * FROM t1 WHERE a = 1
 *   → 将 a = 1 下推到 SeqScan 节点
 *
 * @param plan 计划树
 * @return 优化后的计划树
 */
Plan *opt_predicate_pushdown(Plan *plan);

/**
 * @brief 投影下推
 *
 * 将 SELECT 列表尽可能下推到扫描节点，减少数据传输量。
 *
 * 示例：
 *   SELECT a, b FROM t1
 *   → 只读取 a, b 列而非全表
 *
 * @param plan 计划树
 * @return 优化后的计划树
 */
Plan *opt_projection_pushdown(Plan *plan);

/**
 * @brief 连接顺序优化
 *
 * 基于代价模型选择最优连接顺序。
 *
 * 示例：
 *   t1 JOIN t2 JOIN t3
 *   → 根据表大小和连接条件选择最优顺序
 *
 * @param plan 计划树
 * @return 优化后的计划树
 */
Plan *opt_join_order(Plan *plan);

/**
 * @brief 常量折叠
 *
 * 编译期计算常量表达式，避免运行时重复计算。
 *
 * 示例：
 *   WHERE 1 + 2 = 3
 *   → WHERE true
 *
 * @param plan 计划树
 * @return 优化后的计划树
 */
Plan *opt_constant_folding(Plan *plan);

/**
 * @brief 子查询展开
 *
 * 将相关子查询转换为连接操作。
 *
 * 示例：
 *   SELECT * FROM t1 WHERE a IN (SELECT b FROM t2)
 *   → SELECT * FROM t1 JOIN t2 ON t1.a = t2.b
 *
 * @param plan 计划树
 * @return 优化后的计划树
 */
Plan *opt_subquery_unnest(Plan *plan);

/**
 * @brief 聚合下推
 *
 * 将聚合操作尽可能下推到扫描节点。
 *
 * 示例：
 *   SELECT SUM(a) FROM t1 GROUP BY b
 *   → 可以在扫描时进行部分聚合
 *
 * @param plan 计划树
 * @return 优化后的计划树
 */
Plan *opt_agg_pushdown(Plan *plan);

/**
 * @brief 排序优化
 *
 * 消除不必要的排序操作。
 *
 * 示例：
 *   如果索引已经按所需顺序返回数据，则无需额外排序
 *
 * @param plan 计划树
 * @return 优化后的计划树
 */
Plan *opt_sort_elimination(Plan *plan);

/**
 * @brief 限制下推
 *
 * 将 LIMIT 下推到扫描节点，减少处理的数据量。
 *
 * 示例：
 *   SELECT * FROM t1 LIMIT 10
 *   → 扫描节点可以在获取 10 行后停止
 *
 * @param plan 计划树
 * @return 优化后的计划树
 */
Plan *opt_limit_pushdown(Plan *plan);

#ifdef __cplusplus
}
#endif

#endif  // DB_SQL_OPTIMIZER_H
