/**
 * @file nodetags.h
 * @brief SQL 节点类型标签（执行器层入口）
 *
 * 本文件是执行器层节点标签的入口。Task 1.1 在 parsenodes.h 的 NodeTag 枚举中
 * 已经加入了解析期与内存上下文的标签。本任务（Task 1.2）在此枚举中继续追加
 * 执行器所需的计划/状态/表达式节点标签。
 *
 * 本文件本身不再重复定义 NodeTag 枚举（避免重复 typedef），仅：
 *   1. 包含 parsenodes.h，确保使用方拿到完整的 NodeTag 定义；
 *   2. 集中提供执行器层其他公共类型/宏的前向声明，避免分散在多个头文件。
 *
 * 使用规范：
 *   - 任何要使用 T_PlanState、T_EState 等标签的结构体，include 本文件即可
 *   - 任何要使用 PlanState/EState/ExprContext/TupleTableSlot 的代码，
 *     include execnodes.h。
 */

#ifndef DB_SQL_NODES_NODETAGS_H
#define DB_SQL_NODES_NODETAGS_H

#include <stdint.h>
#include <stdbool.h>

/* 解析期 + 执行器扩展标签的完整定义 */
#include "db/parser/sql/parsenodes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Datum / Size 等基础类型
 *
 * 复用以避免执行器头文件与 parsenodes.h / memctx.h 之间的循环依赖。
 * ======================================================================== */

/** Datum 是通用的"无类型"值容器，与 PG 风格保持一致 */
typedef uint64_t Datum;

/** 字节大小类型 */
typedef unsigned long Size;

/* ========================================================================
 * 扫描方向常量
 * ======================================================================== */

/** 向前扫描 */
#define ForwardScanDirection   0

/** 向后扫描 */
#define BackwardScanDirection  1

/* ========================================================================
 * Var 节点类型常量（占位常量，后续任务会扩展）
 * ======================================================================== */

/** Join 内表变量 */
#define INNER_VAR  1

/** Join 外表变量 */
#define OUTER_VAR  2

/** Scan 变量 */
#define SCAN_VAR   3

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODES_NODETAGS_H */
