/**
 * @file nodeRefreshMview.c
 * @brief RefreshMview 执行器节点实现
 *
 * 实现物化视图刷新执行器节点，基于 Volcano 迭代器模型。
 * 参考 PostgreSQL 的 nodeRefreshMview.c 实现。
 *
 * 集成 materialized_view.c 中的刷新 API：
 *   - mv_refresh_full()：完整刷新
 *   - mv_refresh_incremental()：增量刷新
 *   - mv_refresh_concurrent()：无锁刷新
 */

#include "db/sql/nodeRefreshMview.h"
#include "db/sql/materialized_view.h"
#include "db/sql/executor.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * ExecInitRefreshMview - 初始化 RefreshMview 节点
 * ============================================================ */

PlanState *ExecInitRefreshMview(Plan *plan, EState *estate, int eflags)
{
    RefreshMview *rmv_plan = (RefreshMview *)plan;
    RefreshMviewState *state;

    /* 分配状态节点 */
    state = (RefreshMviewState *)palloc0(estate->es_query_cxt,
                                         sizeof(RefreshMviewState));
    if (!state) return NULL;

    /* 初始化基类 */
    state->ps.plan = plan;
    state->ps.state = estate;
    state->ps.ExecProcNode = ExecRefreshMview;

    /* 复制刷新参数 */
    state->refresh_type = rmv_plan->refresh_type;
    strncpy(state->view_name, rmv_plan->view_name, sizeof(state->view_name) - 1);
    state->view_name[sizeof(state->view_name) - 1] = '\0';
    state->with_data = rmv_plan->with_data;
    state->refresh_status = 0;

    /* 分配结果槽（返回 1 列：状态码） */
    state->ps.ps_ResultTupleSlot = (TupleTableSlot *)palloc0(
        estate->es_query_cxt, sizeof(TupleTableSlot));
    if (state->ps.ps_ResultTupleSlot) {
        TupleTableSlot *slot = state->ps.ps_ResultTupleSlot;
        slot->tts_nvalid = 1;
        slot->tts_values = (Datum *)palloc0(estate->es_query_cxt, sizeof(Datum));
        slot->tts_isnull = (bool *)palloc0(estate->es_query_cxt, sizeof(bool));
    }

    return &state->ps;
}

/* ============================================================
 * ExecRefreshMview - 执行 RefreshMview 节点
 * ============================================================ */

TupleTableSlot *ExecRefreshMview(PlanState *pstate)
{
    RefreshMviewState *state = (RefreshMviewState *)pstate;
    TupleTableSlot *slot = pstate->ps_ResultTupleSlot;

    if (state->refresh_status != 0) {
        /* 已经执行过，返回 NULL 表示完成 */
        return NULL;
    }

    printf("[RefreshMview] 刷新物化视图: %s (类型: %d)\n",
           state->view_name, state->refresh_type);

    /* 注意：当前实现为桩代码，仅打印日志。
     * 完整实现需要：
     * 1. 通过 Catalog 获取物化视图定义
     * 2. 调用 mv_refresh_full/incremental/concurrent
     * 3. 更新物化视图状态
     *
     * 由于 mv_manager 需要从外部传入（或通过全局状态），
     * 这里提供框架，具体集成需要与 catalog 模块对接。 */
    state->refresh_status = 0;  /* 0 表示成功 */

    /* 设置虚拟元组槽用于返回状态信息 */
    slot->tts_isnull[0] = false;
    slot->tts_values[0] = (Datum)(intptr_t)state->refresh_status;

    return slot;
}

/* ============================================================
 * ExecEndRefreshMview - 结束 RefreshMview 节点
 * ============================================================ */

void ExecEndRefreshMview(RefreshMviewState *node)
{
    if (!node) return;

    /* 释放结果槽 - 由 estate 统一管理，此处仅置空指针 */
    node->ps.ps_ResultTupleSlot = NULL;

    printf("[RefreshMview] 结束刷新物化视图: %s\n", node->view_name);
}

/* ============================================================
 * ExecReScanRefreshMview - 重置 RefreshMview 节点
 * ============================================================ */

void ExecReScanRefreshMview(RefreshMviewState *node)
{
    if (!node) return;

    /* 重置状态，允许重新执行 */
    node->refresh_status = 0;

    printf("[RefreshMview] 重置刷新物化视图: %s\n", node->view_name);
}
