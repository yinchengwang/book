/**
 * @file nodeSort.c
 * @brief Sort 排序执行器节点实现
 *
 * 实现 Task 2.5 的 Sort 排序节点：
 *   - ExecInitSort: 初始化 SortState
 *   - ExecSort: 执行排序计算
 *   - ExecEndSort: 释放资源
 *   - ExecReScanSort: 重置节点
 *
 * 实现两阶段排序：
 *   1. 收集阶段：从子节点拉取所有元组
 *   2. 输出阶段：对元组排序并逐个输出
 */

#include "db/sql/nodeSort.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ========================================================================
 * Sort 扩展状态（内部使用）
 * ======================================================================== */

/**
 * @brief Sort 扩展执行状态
 *
 * 维护排序所需的运行时状态，包括：
 *   - 元组数组（收集阶段）
 *   - 排序状态（输出阶段）
 */
typedef struct SortExtState {
    /* 子节点 */
    PlanState *sort_child;           /**< 子计划节点 */

    /* 排序缓冲 */
    void **sort_tuples;              /**< 元组数组 */
    int sort_tupleCount;            /**< 元组数量 */
    int sort_tupleCapacity;         /**< 元组数组容量 */
    int sort_current;                /**< 当前输出位置 */

    /* 排序信息 */
    int numSortCols;                /**< 排序列数 */
    int *sortColIdx;                /**< 排序列索引 */
    Oid *sortOperators;             /**< 排序操作符 */
    bool *nullsFirst;               /**< NULL 值排序位置 */

    /* 元组描述符 */
    struct TupleDescData *sort_tupDesc; /**< 元组描述符 */

    /* 状态标志 */
    bool sort_finished;             /**< 是否已完成排序 */
    bool sort_begun;                /**< 是否已开始 */
} SortExtState;

/* 全局变量用于 qsort 比较函数（避免传递用户数据） */
static SortExtState *g_sort_ext_for_compare = NULL;

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 获取 Sort 扩展状态
 */
static SortExtState *GetSortExtState(SortState *node) {
    if (node == NULL) return NULL;
    /* 扩展状态存储在 ps.ps_ExprContext->ecxt_per_tuple_memory 的句柄中 */
    return (SortExtState *)node->ps.plan;  /* 临时方案：使用 plan 字段存储 */
}

/**
 * @brief 设置 Sort 扩展状态
 */
static void SetSortExtState(SortState *node, SortExtState *ext) {
    if (node == NULL) return;
    node->ps.plan = (Plan *)ext;  /* 临时方案 */
}

/**
 * @brief 比较两个元组（用于 qsort）
 *
 * 按照 numSortCols 指定的列进行排序。
 *
 * @param a 第一个元组（TupleTableSlot*）
 * @param b 第二个元组（TupleTableSlot*）
 * @return 比较结果（<0, 0, >0）
 */
static int sort_compare(const void *a, const void *b) {
    TupleTableSlot *slot_a = (TupleTableSlot *)a;
    TupleTableSlot *slot_b = (TupleTableSlot *)b;
    SortExtState *ext = g_sort_ext_for_compare;

    if (ext == NULL) {
        return 0;
    }

    /* 比较每列 */
    for (int i = 0; i < ext->numSortCols; i++) {
        int col_idx = ext->sortColIdx[i];
        Datum val_a, val_b;
        bool isnull_a, isnull_b;

        /* 获取元组值 */
        if (slot_a->tts_values && col_idx < slot_a->tts_nvalid) {
            val_a = slot_a->tts_values[col_idx];
            isnull_a = slot_a->tts_isnull ? slot_a->tts_isnull[col_idx] : false;
        } else {
            val_a = 0;
            isnull_a = true;
        }

        if (slot_b->tts_values && col_idx < slot_b->tts_nvalid) {
            val_b = slot_b->tts_values[col_idx];
            isnull_b = slot_b->tts_isnull ? slot_b->tts_isnull[col_idx] : false;
        } else {
            val_b = 0;
            isnull_b = true;
        }

        /* NULL 处理 */
        if (isnull_a && isnull_b) {
            continue;
        }
        if (isnull_a) {
            return ext->nullsFirst[i] ? -1 : 1;
        }
        if (isnull_b) {
            return ext->nullsFirst[i] ? 1 : -1;
        }

        /* 数值比较（假设为整数类型） */
        if (val_a < val_b) {
            return -1;
        } else if (val_a > val_b) {
            return 1;
        }
        /* 相等，继续下一列 */
    }

    return 0;
}

/**
 * @brief 复制元组槽数据
 */
static TupleTableSlot *copy_tuple_slot(TupleTableSlot *src, MemoryContext ctx) {
    if (src == NULL) return NULL;

    TupleTableSlot *dst = (TupleTableSlot *)palloc(ctx, sizeof(TupleTableSlot));
    if (dst == NULL) return NULL;

    memset(dst, 0, sizeof(TupleTableSlot));
    dst->type = T_TupleTableSlot;

    /* 复制元组描述符 */
    if (src->tts_tupleDescriptor) {
        dst->tts_tupleDescriptor = (struct TupleDescData *)palloc(ctx, sizeof(struct TupleDescData));
        memcpy(dst->tts_tupleDescriptor, src->tts_tupleDescriptor, sizeof(struct TupleDescData));
    }

    /* 复制值数组 */
    int natts = src->tts_tupleDescriptor ? src->tts_tupleDescriptor->natts : 0;
    if (natts > 0) {
        dst->tts_values = (Datum *)palloc(ctx, sizeof(Datum) * natts);
        dst->tts_isnull = (bool *)palloc(ctx, sizeof(bool) * natts);
        memcpy(dst->tts_values, src->tts_values, sizeof(Datum) * natts);
        memcpy(dst->tts_isnull, src->tts_isnull, sizeof(bool) * natts);
    }

    dst->tts_nvalid = src->tts_nvalid;
    dst->tts_shouldFree = false;
    dst->tts_tuple = src->tts_tuple;

    return dst;
}

/**
 * @brief 分配元组数组
 */
static void **alloc_tuple_array(int capacity, MemoryContext ctx) {
    return (void **)palloc(ctx, sizeof(void *) * capacity);
}

/**
 * @brief 扩展元组数组容量
 */
static void **realloc_tuple_array(void **arr, int old_cap, int new_cap, MemoryContext ctx) {
    void **new_arr = (void **)palloc(ctx, sizeof(void *) * new_cap);
    if (arr && old_cap > 0) {
        memcpy(new_arr, arr, sizeof(void *) * old_cap);
        pfree(ctx, arr);
    }
    return new_arr;
}

/* ========================================================================
 * Sort 节点执行函数
 * ======================================================================== */

/**
 * @brief Sort 节点执行函数（内部实现）
 *
 * Sort 算法（两阶段）：
 *   1. 收集阶段：从子节点拉取所有元组，存入数组
 *   2. 输出阶段：对数组排序，逐个输出
 *
 * @param pstate PlanState（实际类型为 SortState）
 *
 * @return 结果元组槽；无更多元组时返回 NULL
 */
static TupleTableSlot *exec_sort_impl(PlanState *pstate) {
    SortState *node = (SortState *)pstate;
    SortExtState *ext;
    TupleTableSlot *slot;

    if (node == NULL) {
        return NULL;
    }

    ext = GetSortExtState(node);
    if (ext == NULL) {
        return NULL;
    }

    /* 如果还未开始，进入收集阶段 */
    if (!ext->sort_begun) {
        MemoryContext ctx;

        /* 获取内存上下文 */
        if (node->ps.state && ((EState *)node->ps.state)->es_query_cxt) {
            ctx = ((EState *)node->ps.state)->es_query_cxt;
        } else {
            /* 使用 per_tuple 上下文 */
            ctx = node->ps.ps_ExprContext->ecxt_per_tuple_memory;
        }

        /* 初始化元组数组 */
        ext->sort_tupleCapacity = 1024;  /* 初始容量 */
        ext->sort_tuples = alloc_tuple_array(ext->sort_tupleCapacity, ctx);
        ext->sort_tupleCount = 0;
        ext->sort_current = 0;

        /* 收集阶段：从子节点拉取所有元组 */
        if (ext->sort_child != NULL) {
            PlanState *child = ext->sort_child;
            while (true) {
                slot = ExecProcNode(child);
                if (slot == NULL) {
                    break;
                }

                /* 扩展数组容量（如果需要） */
                if (ext->sort_tupleCount >= ext->sort_tupleCapacity) {
                    ext->sort_tupleCapacity *= 2;
                    ext->sort_tuples = realloc_tuple_array(
                        ext->sort_tuples,
                        ext->sort_tupleCount,
                        ext->sort_tupleCapacity,
                        ctx
                    );
                }

                /* 复制元组槽 */
                ext->sort_tuples[ext->sort_tupleCount] = copy_tuple_slot(slot, ctx);
                ext->sort_tupleCount++;
            }
        }

        /* 排序阶段：对元组数组排序 */
        if (ext->sort_tupleCount > 0) {
            /* 设置全局变量供比较函数使用 */
            g_sort_ext_for_compare = ext;
            qsort(ext->sort_tuples, ext->sort_tupleCount, sizeof(void *), sort_compare);
            /* 清除全局变量 */
            g_sort_ext_for_compare = NULL;
        }

        ext->sort_begun = true;
    }

    /* 输出阶段：逐个输出排序后的元组 */
    if (ext->sort_current >= ext->sort_tupleCount) {
        ext->sort_finished = true;
        return NULL;
    }

    /* 返回当前元组槽 */
    slot = (TupleTableSlot *)ext->sort_tuples[ext->sort_current];
    ext->sort_current++;

    return slot;
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 Sort 节点
 *
 * 分配 SortState 并初始化字段。
 * 同时初始化子节点。
 *
 * @param plan   计划节点（实际类型为 Sort*）
 * @param estate 执行器状态
 * @param eflags 执行器标志
 *
 * @return 初始化后的 SortState（作为 PlanState*）；失败返回 NULL
 */
PlanState *ExecInitSort(Plan *plan, EState *estate, int eflags) {
    Sort *node;
    SortState *state;
    SortExtState *ext;
    MemoryContext ctx;

    /* 参数检查 */
    if (plan == NULL || estate == NULL) {
        return NULL;
    }

    node = (Sort *)plan;

    /* 获取查询上下文 */
    ctx = estate->es_query_cxt;

    /* 在查询上下文中分配 SortState */
    state = (SortState *)palloc0(ctx, sizeof(SortState));
    if (state == NULL) {
        return NULL;
    }

    /* 分配扩展状态 */
    ext = (SortExtState *)palloc0(ctx, sizeof(SortExtState));
    if (ext == NULL) {
        return NULL;
    }

    /* 初始化基类 */
    state->ps.type = T_SortState;
    state->ps.plan = plan;
    state->ps.state = estate;
    state->ps.ExecProcNode = exec_sort_impl;
    state->ps.ExecProcNodeReal = exec_sort_impl;

    /* 保存扩展状态（临时方案） */
    SetSortExtState(state, ext);

    /* 初始化子节点 */
    if (node->plan.lefttree != NULL) {
        ext->sort_child = ExecInitNode(node->plan.lefttree, estate, eflags);
    } else {
        ext->sort_child = NULL;
    }

    /* Sort 节点无右子树 */
    state->ps.righttree = NULL;

    /* 初始化表达式上下文 */
    state->ps.ps_ExprContext = CreateExprContext(estate);
    if (state->ps.ps_ExprContext == NULL) {
        return NULL;
    }

    /* 创建结果槽 */
    state->ps.ps_ResultTupleSlot = MakeTupleTableSlotWithMCxt(ctx);
    if (state->ps.ps_ResultTupleSlot == NULL) {
        return NULL;
    }

    /* 初始化 Sort 特定字段 */
    state->numCols = node->numCols;
    state->sort_Done = false;
    state->tuplesortstate = NULL;

    /* 初始化扩展状态 */
    ext->numSortCols = node->numCols;
    if (node->numCols > 0 && node->sortColIdx != NULL) {
        ext->sortColIdx = (int *)palloc(ctx, sizeof(int) * node->numCols);
        memcpy(ext->sortColIdx, node->sortColIdx, sizeof(int) * node->numCols);

        ext->sortOperators = (Oid *)palloc(ctx, sizeof(Oid) * node->numCols);
        if (node->sortOperators) {
            memcpy(ext->sortOperators, node->sortOperators, sizeof(Oid) * node->numCols);
        }

        ext->nullsFirst = (bool *)palloc(ctx, sizeof(bool) * node->numCols);
        if (node->nullsFirst) {
            memcpy(ext->nullsFirst, node->nullsFirst, sizeof(bool) * node->numCols);
        }
    }

    ext->sort_tuples = NULL;
    ext->sort_tupleCount = 0;
    ext->sort_tupleCapacity = 0;
    ext->sort_current = 0;
    ext->sort_finished = false;
    ext->sort_begun = false;

    /* 初始化其他字段 */
    state->ps.qual = NULL;
    state->ps.recheck = NULL;
    state->ps.ps_ProjInfo = NULL;
    state->ps.ps_ResultTupleDesc = NULL;
    state->ps.instrument = NULL;
    state->ps.needs_to_scan_queue = false;
    state->ps.chgParam = NULL;

    (void)eflags;

    return (PlanState *)state;
}

/**
 * @brief Sort 节点执行函数（公共接口）
 *
 * @param pstate PlanState（实际类型为 SortState）
 *
 * @return 结果元组槽；无更多元组时返回 NULL
 */
TupleTableSlot *ExecSort(PlanState *pstate) {
    return exec_sort_impl(pstate);
}

/**
 * @brief 结束 Sort 节点
 *
 * 释放 SortState 关联的资源。
 *
 * @param node SortState（可为 NULL）
 */
void ExecEndSort(SortState *node) {
    if (node == NULL) {
        return;
    }

    /* 释放子节点 */
    if (node->ps.lefttree != NULL) {
        ExecEndNode(node->ps.lefttree);
        node->ps.lefttree = NULL;
    }

    /* 释放元组排序状态 */
    if (node->tuplesortstate != NULL) {
        free(node->tuplesortstate);
        node->tuplesortstate = NULL;
    }

    /* 释放表达式上下文 */
    if (node->ps.ps_ExprContext != NULL) {
        FreeExprContext(node->ps.ps_ExprContext, true);
        node->ps.ps_ExprContext = NULL;
    }

    /* 释放结果槽 */
    if (node->ps.ps_ResultTupleSlot != NULL) {
        FreeTupleTableSlot(node->ps.ps_ResultTupleSlot);
        node->ps.ps_ResultTupleSlot = NULL;
    }

    /* 注意：SortState 本身由 EState 的查询上下文管理，不单独释放 */
    (void)node;
}

/**
 * @brief 重置 Sort 节点（用于重新扫描）
 *
 * 重置排序状态，允许重新排序。
 *
 * @param node SortState
 */
void ExecReScanSort(SortState *node) {
    if (node == NULL) {
        return;
    }

    SortExtState *ext = GetSortExtState(node);
    if (ext != NULL) {
        /* 重置扩展状态 */
        ext->sort_tuples = NULL;
        ext->sort_tupleCount = 0;
        ext->sort_tupleCapacity = 0;
        ext->sort_current = 0;
        ext->sort_finished = false;
        ext->sort_begun = false;
    }

    /* 重置子节点 */
    if (node->ps.lefttree != NULL) {
        ExecReScan(node->ps.lefttree);
    }

    /* 重置排序状态 */
    node->sort_Done = false;
}
