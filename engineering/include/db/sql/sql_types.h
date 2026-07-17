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

#ifdef __cplusplus
extern "C" {
#endif

/* 解析期基本类型 */
typedef uint64_t Oid;        /**< 对象标识符类型 */
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

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_TYPES_H */
