/**
 * @file sql_types.h
 * @brief SQL 层共享类型定义
 *
 * 集中管理 SQL 各子模块之间的共享类型定义，避免循环依赖。
 */
#ifndef DB_SQL_TYPES_H
#define DB_SQL_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* NodeTag（struct Expr_s 首字段）。parsenodes.h 只依赖 stdint/stdbool，
 * 不构成循环依赖。 */
#include "db/parser/sql/parsenodes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 解析期基本类型 */
/* T7：Oid 全仓统一为 uint32_t（与 catalog.h/parse_node.h 一致），消除
 * parse_node.h(uint32_t) vs sql_types.h(uint64_t) 的 ABI 级冲突。 */
typedef uint32_t Oid;        /**< 对象标识符类型 */
typedef uint32_t CommandId;  /**< 命令标识符 */

/* ========================================================================
 * 表达式类型（共享定义）
 * ======================================================================== */

/** 表达式类型枚举 */
typedef enum SqlExprType {
    EXPR_CONST,             /**< 常量 */
    EXPR_VAR,               /**< 变量 */
    EXPR_PARAM,             /**< 参数 */
    EXPR_OP,                /**< 操作符 */
    EXPR_FUNC,              /**< 函数 */
    EXPR_AGG,               /**< 聚合函数 */
    EXPR_WINDOW,            /**< 窗口函数 */
    EXPR_CAST,              /**< 类型转换 */
    EXPR_CASE,              /**< CASE 表达式 */
    EXPR_ARRAY,             /**< 数组 */
    EXPR_ARRAY_REF,         /**< 数组引用 */
    EXPR_COALESCE,          /**< COALESCE */
    EXPR_NULLTEST,          /**< NULL 测试 */
    EXPR_BOOLTEST,          /**< 布尔测试 */
    EXPR_BOOL_AND,          /**< 布尔 AND */
    EXPR_BOOL_OR,           /**< 布尔 OR */
    EXPR_BOOL_NOT,          /**< 布尔 NOT */
    EXPR_SUBLINK,           /**< 子链接 */
    EXPR_SUBPLAN,           /**< 子计划 */
    EXPR_PLAN,              /**< 计划引用 */
    EXPR_ALTERNATIVE,       /**< 替代表达式 */
    EXPR_MINMAX,            /**< MIN/MAX */
    EXPR_XML,               /**< XML */
    EXPR_JSON,              /**< JSON */
    EXPR_JSONB,             /**< JSONB */
    EXPR_VECTOR,            /**< 向量表达式 */
    EXPR_TIMESERIES,        /**< 时序表达式 */
    EXPR_GRAPH,             /**< 图表达式 */
    EXPR_DOCUMENT           /**< 文档表达式 */
} SqlExprType;

/* ========================================================================
 * 表达式节点（canonical 定义）
 * ======================================================================== */

/**
 * @brief 表达式（planner/executor 共享的 canonical 定义）
 *
 * T8：由 sql_planner.h 迁入本共享头。原位置使 cost.c 等无法引入
 * sql_planner.h 的 TU（CostParams typedef 名冲突）无法对 Expr 做
 * expr_type 级别的判断（T7 因此丢失了 estimate_selectivity 的常量检查）。
 * 迁入后任何 TU 经 sql_types.h 即可获得完整定义。
 *
 * 注意与 parse_node.h 的组 1 `typedef struct Expr {...} Expr` 分属两个
 * 阵营：经核实没有任何 TU 同时包含 parse_node.h 与本头
 * （parse_node.c/parse_expr.c/parse_analyze.c 均不引 sql_types.h）。
 */
typedef struct Expr_s {
    NodeTag         type;           /**< 节点类型标签 T_Expr */
    SqlExprType     expr_type;      /**< 表达式类型 */
    Oid             result_type;    /**< 结果类型 OID */
    int             result_len;     /**< 结果长度 */
    int             result_by_val;  /**< 是否按值传递 */
    union {
        struct {
            int value;      /**< 常量值 */
            int isnull;     /**< 是否为 NULL */
        } const_val;
        struct {
            int varattno;  /**< 属性编号 */
            int varno;     /**< 变量编号 */
        } var;
        int paramno;       /**< 参数编号 */
        struct {
            int opno;      /**< 操作符 OID */
            struct Expr_s *lexpr;
            struct Expr_s *rexpr;
        } op;
        struct {
            int funcid;    /**< 函数 OID */
            struct Expr_s **args;
            int nargs;
        } func;
    } val;
} Expr;

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_TYPES_H */
