/**
 * @file nodeMergejoin.c
 * @brief W4.5: MergeJoin 执行器节点实现
 *
 * 实现排序归并连接算法：
 *   1. 内外表均已按连接键排序
 *   2. 同时扫描，比较连接键
 *   3. 相等则输出匹配行
 *   4. 不等则推进较小的那一边
 *
 * 参考 PostgreSQL 的 nodeMergejoin.c。
 */

#include "db/sql/nodeMergejoin.h"
#include "db/sql/executor.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 比较两个元组的连接键
 *
 * 简化实现：使用 int 类型的 Datum 比较。
 * 返回 -1/0/1 语义与 memcmp 一致。
 *
 * @param outer_val 外表连接键值
 * @param inner_val 内表连接键值
 * @return -1: outer < inner, 0: equal, 1: outer > inner
 */
static int merge_compare_keys(Datum outer_val, Datum inner_val) {
    int64_t ov = (int64_t)(uintptr_t)outer_val;
    int64_t iv = (int64_t)(uintptr_t)inner_val;

    if (ov < iv) return -1;
    if (ov > iv) return 1;
    return 0;
}

/**
 * @brief 从外表获取下一个元组
 *
 * 从外表子计划拉取下一个元组，存入 mj_OuterTupleSlot。
 * 如果外表已耗尽，设置 mj_OuterDone = true。
 */
static void merge_get_outer_tuple(MergeJoinState *state) {
    TupleTableSlot *slot;

    if (state->mj_OuterDone) {
        return;
    }

    slot = ExecProcNode(state->js.ps.lefttree);
    if (TupIsNull(slot)) {
        state->mj_OuterDone = true;
        return;
    }

    state->mj_OuterTupleSlot = slot;
}

/**
 * @brief 从内表获取下一个元组
 *
 * 从内表子计划拉取下一个元组，存入 mj_InnerTupleSlot。
 * 如果内表已耗尽，设置 mj_InnerDone = true。
 */
static void merge_get_inner_tuple(MergeJoinState *state) {
    TupleTableSlot *slot;

    if (state->mj_InnerDone) {
        return;
    }

    slot = ExecProcNode(state->js.ps.righttree);
    if (TupIsNull(slot)) {
        state->mj_InnerDone = true;
        return;
    }

    state->mj_InnerTupleSlot = slot;
}

/* ========================================================================
 * MergeJoin 执行器实现
 * ======================================================================== */

PlanState *ExecInitMergeJoin(Plan *plan, EState *estate, int eflags) {
    MergeJoinState *state;

    state = (MergeJoinState *)calloc(1, sizeof(MergeJoinState));
    if (!state) {
        return NULL;
    }

    /* 初始化基类 */
    state->js.ps.type = T_MergeJoinState;
    state->js.ps.plan = plan;
    state->js.ps.state = estate;
    PlanStateSetExecProc(&state->js.ps, (ExecProcNodeMtd)ExecMergeJoin);

    /* 初始化连接条件 */
    state->js.joinqual = NULL;

    /* 初始化归并条件 */
    state->mergeclauses = NULL;

    /* 初始化扫描状态 */
    state->merge_initialized = false;
    state->mj_OuterTupleSlot = NULL;
    state->mj_InnerTupleSlot = NULL;
    state->mj_OuterDone = false;
    state->mj_InnerDone = false;
    state->mj_JoinState = 0;

    /* 初始化子节点 */
    if (plan->lefttree) {
        state->js.ps.lefttree = ExecInitNode(plan->lefttree, estate, eflags);
    }
    if (plan->righttree) {
        state->js.ps.righttree = ExecInitNode(plan->righttree, estate, eflags);
    }

    /* 创建结果槽 */
    state->js.ps.ps_ResultTupleSlot = MakeTupleTableSlotWithMCxt(estate->es_query_cxt);

    return (PlanState *)state;
}

TupleTableSlot *ExecMergeJoin(PlanState *pstate) {
    MergeJoinState *state;
    TupleTableSlot *result;

    if (!pstate) {
        return NULL;
    }

    state = (MergeJoinState *)pstate;

    /* 首次执行时初始化：获取内外表的第一个元组 */
    if (!state->merge_initialized) {
        merge_get_outer_tuple(state);
        merge_get_inner_tuple(state);
        state->merge_initialized = true;
    }

    /* 主循环：归并比较内外表元组 */
    while (true) {
        /* 检查是否已耗尽 */
        if (state->mj_OuterDone || state->mj_InnerDone) {
            return NULL;
        }

        /* 获取当前元组的连接键（简化：取第一个值作为连接键） */
        Datum outer_key = (state->mj_OuterTupleSlot &&
                           state->mj_OuterTupleSlot->tts_values)
            ? state->mj_OuterTupleSlot->tts_values[0] : (Datum)0;
        Datum inner_key = (state->mj_InnerTupleSlot &&
                           state->mj_InnerTupleSlot->tts_values)
            ? state->mj_InnerTupleSlot->tts_values[0] : (Datum)0;

        /* 比较连接键 */
        int cmp = merge_compare_keys(outer_key, inner_key);

        if (cmp < 0) {
            /* 外表键 < 内表键：推进外表 */
            merge_get_outer_tuple(state);
        } else if (cmp > 0) {
            /* 外表键 > 内表键：推进内表 */
            merge_get_inner_tuple(state);
        } else {
            /* 键相等：输出匹配行 */
            /* 简化实现：输出外表元组 */
            result = state->mj_OuterTupleSlot;

            /* 推进内外表继续匹配（简化：同时推进） */
            merge_get_outer_tuple(state);
            merge_get_inner_tuple(state);

            return result;
        }
    }
}

void ExecEndMergeJoin(MergeJoinState *node) {
    if (!node) {
        return;
    }

    /* 结束子节点 */
    if (node->js.ps.lefttree) {
        ExecEndNode(node->js.ps.lefttree);
        node->js.ps.lefttree = NULL;
    }
    if (node->js.ps.righttree) {
        ExecEndNode(node->js.ps.righttree);
        node->js.ps.righttree = NULL;
    }

    /* 释放结果槽 */
    if (node->js.ps.ps_ResultTupleSlot) {
        FreeTupleTableSlot(node->js.ps.ps_ResultTupleSlot);
        node->js.ps.ps_ResultTupleSlot = NULL;
    }

    free(node);
}

void ExecReScanMergeJoin(MergeJoinState *node) {
    if (!node) {
        return;
    }

    /* 重置扫描状态 */
    node->merge_initialized = false;
    node->mj_OuterTupleSlot = NULL;
    node->mj_InnerTupleSlot = NULL;
    node->mj_OuterDone = false;
    node->mj_InnerDone = false;
    node->mj_JoinState = 0;

    /* 重置子节点 */
    if (node->js.ps.lefttree) {
        ExecReScan(node->js.ps.lefttree);
    }
    if (node->js.ps.righttree) {
        ExecReScan(node->js.ps.righttree);
    }
}