/**
 * @file nodeSeqscan.h
 * @brief SeqScan 执行器节点
 *
 * SeqScan 节点用于全表扫描，如：
 *   - SELECT * FROM table
 *   - SELECT * FROM table WHERE unindexed_column = value
 *
 * 执行行为：
 *   - 打开表（relation_open）
 *   - 初始化扫描描述符（table_beginscan）
 *   - 每次调用返回下一行
 *   - 扫描结束返回 NULL
 *
 * 本文件是 SQL 执行引擎 Phase 2 核心算子层的第二个任务（Task 2.2）。
 */

#ifndef DB_SQL_NODE_SEQSCAN_H
#define DB_SQL_NODE_SEQSCAN_H

#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * SeqScan 计划节点
 *
 * 简化版定义，完整版在 planner 任务中补充。
 * ======================================================================== */

/**
 * @brief SeqScan 计划节点
 *
 * 用于全表扫描。
 *
 * 设计说明：SeqScan 结构体嵌入 Plan 作为第一个字段，使得 SeqScan* 可以安全转换为 Plan*。
 */
typedef struct SeqScan {
    Plan         plan;               /**< 基类：计划节点（必须作为第一个字段） */
    Oid          scanrelid;          /**< 表 OID（RangeTable 索引） */
} SeqScan;

/* ========================================================================
 * SeqScanState - SeqScan 执行状态
 * ======================================================================== */

/**
 * @brief SeqScan 执行状态
 *
 * 维护 SeqScan 节点的运行时状态。
 */
typedef struct SeqScanState {
    PlanState    ps;                 /**< 基类：计划状态 */
    struct Relation *ss_currentRelation;    /**< 当前扫描的表 */
    struct TableScanDescData *ss_currentScanDesc;   /**< 扫描描述符 */
    TupleTableSlot *ss_ScanTupleSlot;   /**< 扫描结果槽 */
} SeqScanState;

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 SeqScan 节点
 *
 * 分配 SeqScanState 并设置 ExecProcNode 函数指针。
 *
 * @param plan   计划节点（实际类型为 SeqScan*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 SeqScanState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitSeqScan(Plan *plan, EState *estate, int eflags);

/**
 * @brief SeqScan 节点执行函数
 *
 * 每次调用返回下一行，扫描结束返回 NULL。
 *
 * @param pstate PlanState（实际类型为 SeqScanState）
 *
 * @return 结果元组槽；无更多元组时返回 NULL
 */
TupleTableSlot *ExecSeqScan(PlanState *pstate);

/**
 * @brief 结束 SeqScan 节点
 *
 * 释放 SeqScanState 关联的资源。
 *
 * @param node SeqScanState（可为 NULL）
 */
void ExecEndSeqScan(SeqScanState *node);

/**
 * @brief 重置 SeqScan 节点（用于重新扫描）
 *
 * 重新初始化扫描描述符，允许重新扫描表。
 *
 * @param node SeqScanState
 */
void ExecReScanSeqScan(SeqScanState *node);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODE_SEQSCAN_H */