/**
 * @file nodeSort.h
 * @brief Sort 排序执行器节点
 *
 * Sort 节点用于排序查询，如：
 *   - SELECT * FROM table ORDER BY column
 *   - SELECT * FROM table ORDER BY column1, column2
 *
 * 执行行为：
 *   - 拉取所有子节点元组
 *   - 在内存或磁盘上排序
 *   - 按顺序返回排序结果
 *
 * 本文件是 SQL 执行引擎 Phase 2 核心算子层的 Task 2.5。
 */

#ifndef DB_SQL_NODE_SORT_H
#define DB_SQL_NODE_SORT_H

#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/expr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Sort 计划节点
 * ======================================================================== */

/**
 * @brief Sort 计划节点
 *
 * 用于排序查询。
 *
 * 设计说明：Sort 结构体嵌入 Plan 作为第一个字段，使得 Sort* 可以安全转换为 Plan*。
 */
typedef struct Sort {
    Plan         plan;               /**< 基类：计划节点（必须作为第一个字段） */
    int          numCols;            /**< 排序列数 */
    int         *sortColIdx;         /**< 排序列索引数组 */
    Oid         *sortOperators;      /**< 排序操作符数组 */
    bool        *nullsFirst;         /**< NULL 值排序位置数组 */
} Sort;

/* ========================================================================
 * SortState - Sort 执行状态
 * ======================================================================== */

/**
 * @brief Sort 执行状态
 *
 * 维护 Sort 节点的运行时状态。
 */
typedef struct SortState {
    PlanState    ps;                 /**< 做类：计划状态 */
    int          numCols;            /**< 排序列数 */
    int         *sortColIdx;         /**< 排序列索引 */
    bool         sort_Done;          /**< 排序完成标志 */
    void        *tuplesortstate;     /**< 元组排序状态（框架版本：未使用） */
} SortState;

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 Sort 节点
 *
 * 分配 SortState 并设置 ExecProcNode 函数指针。
 * 同时初始化子节点。
 *
 * @param plan   计划节点（实际类型为 Sort*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 SortState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitSort(Plan *plan, EState *estate, int eflags);

/**
 * @brief Sort 节点执行函数
 *
 * 执行排序计算。
 * 当前为框架实现，返回 NULL。
 *
 * @param pstate PlanState（实际类型为 SortState）
 *
 * @return 结果元组槽；无更多元组时返回 NULL
 */
TupleTableSlot *ExecSort(PlanState *pstate);

/**
 * @brief 结束 Sort 节点
 *
 * 释放 SortState 关联的资源。
 *
 * @param node SortState（可为 NULL）
 */
void ExecEndSort(SortState *node);

/**
 * @brief 重置 Sort 节点（用于重新扫描）
 *
 * 重置排序状态，允许重新排序。
 *
 * @param node SortState
 */
void ExecReScanSort(SortState *node);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODE_SORT_H */