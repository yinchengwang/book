/**
 * @file nodeRefreshMview.h
 * @brief RefreshMview 执行器节点
 *
 * RefreshMview 节点用于刷新物化视图，如：
 *   - REFRESH MATERIALIZED VIEW view_name
 *   - REFRESH MATERIALIZED VIEW CONCURRENTLY view_name
 *
 * 执行行为：
 *   - 根据刷新类型（FULL/INCREMENTAL/CONCURRENTLY）执行刷新
 *   - 调用 materialized_view.c 中的刷新 API
 *   - 返回刷新状态
 *
 * 本文件是多模态能力补齐 P1-5 物化视图与 SQL 执行器的集成。
 */

#ifndef DB_SQL_NODE_REFRESH_MVIEW_H
#define DB_SQL_NODE_REFRESH_MVIEW_H

#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * RefreshMview 计划节点
 * ======================================================================== */

/**
 * @brief RefreshMview 计划节点
 *
 * 用于刷新物化视图。
 *
 * 设计说明：RefreshMview 结构体嵌入 Plan 作为第一个字段，
 * 使得 RefreshMview* 可以安全转换为 Plan*。
 */
typedef struct RefreshMview {
    Plan         plan;               /**< 基类：计划节点（必须作为第一个字段） */
    int          refresh_type;       /**< 刷新类型：0=FULL, 1=INCREMENTAL, 2=CONCURRENTLY */
    char         view_name[128];     /**< 物化视图名称 */
    bool         with_data;          /**< 是否物化数据 */
} RefreshMview;

/* ========================================================================
 * RefreshMviewState - RefreshMview 执行状态
 * ======================================================================== */

/**
 * @brief RefreshMview 执行状态
 *
 * 维护 RefreshMview 节点的运行时状态。
 */
typedef struct RefreshMviewState {
    PlanState    ps;                 /**< 基类：计划状态 */
    int          refresh_type;       /**< 刷新类型 */
    char         view_name[128];     /**< 物化视图名称 */
    bool         with_data;          /**< 是否物化数据 */
    int          refresh_status;     /**< 刷新状态：0=成功，非0=失败 */
} RefreshMviewState;

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 RefreshMview 节点
 *
 * 分配 RefreshMviewState 并设置 ExecProcNode 函数指针。
 *
 * @param plan   计划节点（实际类型为 RefreshMview*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 RefreshMviewState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitRefreshMview(Plan *plan, EState *estate, int eflags);

/**
 * @brief RefreshMview 节点执行函数
 *
 * 执行物化视图刷新操作。
 *
 * @param pstate PlanState（实际类型为 RefreshMviewState）
 *
 * @return 结果元组槽；刷新完成返回 NULL
 */
TupleTableSlot *ExecRefreshMview(PlanState *pstate);

/**
 * @brief 结束 RefreshMview 节点
 *
 * 释放 RefreshMviewState 关联的资源。
 *
 * @param node RefreshMviewState（可为 NULL）
 */
void ExecEndRefreshMview(RefreshMviewState *node);

/**
 * @brief 重置 RefreshMview 节点（用于重新扫描）
 *
 * @param node RefreshMviewState
 */
void ExecReScanRefreshMview(RefreshMviewState *node);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODE_REFRESH_MVIEW_H */
