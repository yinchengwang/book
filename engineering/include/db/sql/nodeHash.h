/**
 * @file nodeHash.h
 * @brief Hash 辅助执行器节点
 *
 * Hash 节点用于 HashJoin 的构建阶段：
 *   - 扫描内表（小表）
 *   - 构建哈希表（key = 连接键）
 *   - 为探测阶段提供哈希表
 *
 * 执行行为：
 *   - 首次调用时构建哈希表
 *   - 后续调用返回 NULL（Hash 不产生输出元组）
 *
 * 本文件是 SQL 执行引擎 Phase 2 核心算子层的第三个任务（Task 2.3）。
 */

#ifndef DB_SQL_NODE_HASH_H
#define DB_SQL_NODE_HASH_H

#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Hash 计划节点
 *
 * 简化版定义，完整版在 planner 任务中补充。
 * ======================================================================== */

/**
 * @brief Hash 计划节点（辅助节点）
 *
 * 用于 HashJoin 的构建阶段。
 *
 * 设计说明：Hash 结构体嵌入 Plan 作为第一个字段，使得 Hash* 可以安全转换为 Plan*。
 */
typedef struct Hash {
    Plan         plan;               /**< 基类：计划节点（必须作为第一个字段） */
    Oid          hashoperator;       /**< 哈希操作符（等值判断函数 OID） */
    List        *hashkeys;           /**< 哈希键表达式列表 */
} Hash;

/* ========================================================================
 * HashState - Hash 执行状态
 * ======================================================================== */

/**
 * @brief Hash 执行状态
 *
 * 维护 Hash 节点的运行时状态。
 */
typedef struct HashState {
    PlanState    ps;                 /**< 基类：计划状态 */
    void        *hashtable;          /**< 哈希表（简化版，后续实现） */
    int          hashsize;           /**< 哈希表大小 */
} HashState;

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 Hash 节点
 *
 * 分配 HashState 并设置 ExecProcNode 函数指针。
 *
 * @param plan   计划节点（实际类型为 Hash*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 HashState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitHash(Plan *plan, EState *estate, int eflags);

/**
 * @brief 结束 Hash 节点
 *
 * 释放 HashState 关联的资源。
 *
 * @param node HashState（可为 NULL）
 */
void ExecEndHash(HashState *node);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODE_HASH_H */