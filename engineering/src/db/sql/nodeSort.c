/**
 * @file nodeSort.c
 * @brief Sort 排序执行器节点实现
 *
 * 实现 Sort 排序节点（两阶段 Volcano）：
 *   1. 收集阶段：从子节点拉取所有元组，复制到内部缓冲
 *   2. 输出阶段：对元组按 sortColIdx / sortColTypes / sortDesc 排序，逐个返回
 *
 * T9 修订要点：
 *   - exec_sort_impl 重写：qsort 比较器修正（元素为 TupleTableSlot**，
 *     比较器接收的是 (TupleTableSlot**)*），按 sortColTypes 类型感知比较
 *     （int 家族按 int64_t；text 家族按 strcmp），按 sortDesc 翻转方向。
 *   - copy_tuple_slot 简化：共享描述符指针（避开放位/真身布局不一致——
 *     见 T8 报告 §1.3），Datum/isnull 数组按 tts_nvalid 复制（适配 SeqScan
 *     等设置 nvalid=natts 的算子）。
 *   - ExecInitSort 复制 sortDesc/sortColTypes 到 SortState，并显式设置
 *     ps.lefttree 使 ExecEndNode 递归能走到子节点（避免 SeqScanState 泄漏）。
 *   - 排序输出槽 ps_ResultTupleSlot 仍由框架分配（passthrough，不填充
 *     值；驱动只使用子节点拉取到的真实 slot）。
 */

#include "db/sql/nodeSort.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ========================================================================
 * 驱动层 OID（与 sql_driver.c 一致；避免跨 TU 互相 include）
 * ======================================================================== */

#define SORT_OID_INT2    21
#define SORT_OID_INT4    23
#define SORT_OID_INT8    20
#define SORT_OID_TEXT    25
#define SORT_OID_CHAR    1042
#define SORT_OID_VARCHAR 1043

static inline bool sort_oid_is_text(Oid t) {
    return t == SORT_OID_TEXT || t == SORT_OID_CHAR || t == SORT_OID_VARCHAR;
}

/* ========================================================================
 * Sort 扩展状态（内部使用）
 * ======================================================================== */

typedef struct SortExtState {
    SortState *sort_state;          /**< 回指 SortState（qsort 比较器取列元数据） */
    void    **sort_tuples;            /**< TupleTableSlot* 数组 */
    int       sort_tupleCount;
    int       sort_tupleCapacity;
    int       sort_current;           /**< 当前输出位置 */
    bool      sort_finished;
    bool      sort_begun;
} SortExtState;

/* qsort 比较函数用：避免向 qsort 传用户数据 */
static SortExtState *g_sort_ext_for_compare = NULL;

/* ========================================================================
 * 比较与复制
 * ======================================================================== */

static int sort_compare(const void *a, const void *b) {
    /* a, b 指向 sort_tuples[] 元素；元素类型为 TupleTableSlot* */
    TupleTableSlot *sa = *(TupleTableSlot *const *)a;
    TupleTableSlot *sb = *(TupleTableSlot *const *)b;
    SortExtState *ext = g_sort_ext_for_compare;
    if (sa == NULL || sb == NULL || ext == NULL || ext->sort_state == NULL) {
        return 0;
    }

    SortState *st = ext->sort_state;
    int n = st->numCols;
    int *idx = st->sortColIdx;
    Oid *types = st->sortColTypes;
    bool *desc = st->sortDesc;
    bool *nf = st->ps.plan ? NULL : NULL;  /* 暂未使用 nullsFirst */

    for (int i = 0; i < n; i++) {
        int c = idx[i];
        if (sa->tts_values == NULL || sb->tts_values == NULL ||
            c >= sa->tts_nvalid || c >= sb->tts_nvalid) {
            continue;
        }
        Datum va = sa->tts_values[c];
        Datum vb = sb->tts_values[c];
        bool na = sa->tts_isnull ? sa->tts_isnull[c] : false;
        bool nb = sb->tts_isnull ? sb->tts_isnull[c] : false;

        int cmp;
        if (na && nb) {
            continue;  /* 双 NULL → 视为相等，下一列 */
        }
        if (na) {
            cmp = (nf && nf[i]) ? -1 : 1;
        } else if (nb) {
            cmp = (nf && nf[i]) ? 1 : -1;
        } else if (types != NULL && sort_oid_is_text(types[i])) {
            const char *sa_s = va ? (const char *)(uintptr_t)va : "";
            const char *sb_s = vb ? (const char *)(uintptr_t)vb : "";
            cmp = strcmp(sa_s, sb_s);
        } else {
            int64_t la = (int64_t)va;
            int64_t lb = (int64_t)vb;
            if (la < lb) cmp = -1;
            else if (la > lb) cmp = 1;
            else cmp = 0;
        }
        if (desc != NULL && desc[i]) {
            cmp = -cmp;
        }
        if (cmp != 0) {
            return cmp;
        }
    }
    return 0;
}

/**
 * @brief 深拷贝一个元组槽（共享描述符；复制值/空位数组）
 *
 * 描述符在查询周期内由 storage/catalog 维护，共享即可；
 * Datum 数组按 tts_nvalid 大小复制，文本类 Datum 指向 es_query_cxt 内
 * 已分配内存（随 EState 整体回收，无需单独处理）。
 */
static TupleTableSlot *copy_tuple_slot(TupleTableSlot *src, MemoryContext ctx) {
    if (src == NULL) return NULL;
    TupleTableSlot *dst = (TupleTableSlot *)palloc(ctx, sizeof(TupleTableSlot));
    if (dst == NULL) return NULL;
    memset(dst, 0, sizeof(TupleTableSlot));
    dst->type = T_TupleTableSlot;

    /* 共享描述符指针（不拷贝结构体——避开放位/真身布局差异） */
    dst->tts_tupleDescriptor = src->tts_tupleDescriptor;
    dst->tts_ops = src->tts_ops;

    int nvalid = src->tts_nvalid > 0 ? src->tts_nvalid : 0;
    if (nvalid > 0) {
        dst->tts_values = (Datum *)palloc(ctx, sizeof(Datum) * nvalid);
        dst->tts_isnull = (bool *)palloc(ctx, sizeof(bool) * nvalid);
        if (src->tts_values != NULL) {
            memcpy(dst->tts_values, src->tts_values, sizeof(Datum) * nvalid);
        }
        if (src->tts_isnull != NULL) {
            memcpy(dst->tts_isnull, src->tts_isnull, sizeof(bool) * nvalid);
        }
        dst->tts_nvalid = nvalid;
    }
    dst->tts_shouldFree = false;
    dst->tts_shouldFreeMin = false;
    return dst;
}

static void **realloc_tuple_array(void **arr, int new_cap, MemoryContext ctx) {
    return (void **)palloc(ctx, sizeof(void *) * new_cap);
}

/* ========================================================================
 * Sort 节点执行函数
 * ======================================================================== */

static TupleTableSlot *exec_sort_impl(PlanState *pstate) {
    SortState *node = (SortState *)pstate;
    SortExtState *ext = (SortExtState *)node->ps.plan;  /* ext 借存 ps.plan */
    TupleTableSlot *slot;

    if (node == NULL || ext == NULL) {
        return NULL;
    }

    if (!ext->sort_begun) {
        MemoryContext ctx;
        if (node->ps.state && ((EState *)node->ps.state)->es_query_cxt) {
            ctx = ((EState *)node->ps.state)->es_query_cxt;
        } else {
            ctx = node->ps.ps_ExprContext
                ? node->ps.ps_ExprContext->ecxt_per_tuple_memory
                : MemoryContextCurrent();
        }

        ext->sort_tupleCapacity = 64;
        ext->sort_tuples = realloc_tuple_array(NULL, ext->sort_tupleCapacity, ctx);
        ext->sort_tupleCount = 0;
        ext->sort_current = 0;
        ext->sort_finished = false;

        /* 收集阶段：从子节点拉取所有元组并深拷贝 */
        PlanState *child = node->ps.lefttree;
        if (child != NULL) {
            while ((slot = ExecProcNode(child)) != NULL) {
                if (ext->sort_tupleCount >= ext->sort_tupleCapacity) {
                    ext->sort_tupleCapacity *= 2;
                    ext->sort_tuples = realloc_tuple_array(
                        ext->sort_tuples, ext->sort_tupleCapacity, ctx);
                }
                ext->sort_tuples[ext->sort_tupleCount++] = copy_tuple_slot(slot, ctx);
                ExecClearTuple(slot);
            }
        }

        /* 排序阶段 */
        if (ext->sort_tupleCount > 1) {
            g_sort_ext_for_compare = ext;
            qsort(ext->sort_tuples, (size_t)ext->sort_tupleCount,
                  sizeof(void *), sort_compare);
            g_sort_ext_for_compare = NULL;
        }
        ext->sort_begun = true;
    }

    if (ext->sort_current >= ext->sort_tupleCount) {
        ext->sort_finished = true;
        return NULL;  /* 本节点语义完成：必须返回 NULL（绝不返回空 slot） */
    }

    slot = (TupleTableSlot *)ext->sort_tuples[ext->sort_current++];
    return slot;
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

PlanState *ExecInitSort(Plan *plan, EState *estate, int eflags) {
    Sort *node;
    SortState *state;
    SortExtState *ext;
    MemoryContext ctx;

    if (plan == NULL || estate == NULL) {
        return NULL;
    }
    node = (Sort *)plan;
    ctx = estate->es_query_cxt;

    state = (SortState *)palloc0(ctx, sizeof(SortState));
    if (state == NULL) return NULL;

    ext = (SortExtState *)palloc0(ctx, sizeof(SortExtState));
    if (ext == NULL) return NULL;

    state->ps.type = T_SortState;
    state->ps.plan = plan;
    state->ps.state = estate;
    state->ps.ExecProcNode = exec_sort_impl;
    state->ps.ExecProcNodeReal = exec_sort_impl;

    /* ext 借存于 ps.plan 字段——执行期通过该字段取回 */
    state->ps.plan = (Plan *)ext;

    state->numCols = node->numCols;
    state->sort_Done = false;
    state->tuplesortstate = NULL;

    if (node->numCols > 0) {
        if (node->sortColIdx != NULL) {
            state->sortColIdx = (int *)palloc(ctx, sizeof(int) * node->numCols);
            memcpy(state->sortColIdx, node->sortColIdx, sizeof(int) * node->numCols);
        }
        if (node->sortDesc != NULL) {
            state->sortDesc = (bool *)palloc(ctx, sizeof(bool) * node->numCols);
            memcpy(state->sortDesc, node->sortDesc, sizeof(bool) * node->numCols);
        }
        if (node->sortColTypes != NULL) {
            state->sortColTypes = (Oid *)palloc(ctx, sizeof(Oid) * node->numCols);
            memcpy(state->sortColTypes, node->sortColTypes,
                   sizeof(Oid) * node->numCols);
        }
    }

    /* 初始化子节点；T9：同时设置 ps.lefttree，使 ExecEndNode 递归能到达 */
    if (node->plan.lefttree != NULL) {
        state->ps.lefttree = ExecInitNode(node->plan.lefttree, estate, eflags);
    } else {
        state->ps.lefttree = NULL;
    }
    state->ps.righttree = NULL;

    state->ps.ps_ExprContext = CreateExprContext(estate);
    if (state->ps.ps_ExprContext == NULL) return NULL;

    state->ps.ps_ResultTupleSlot = MakeTupleTableSlotWithMCxt(ctx);
    if (state->ps.ps_ResultTupleSlot == NULL) return NULL;

    state->ps.qual = NULL;
    state->ps.recheck = NULL;
    state->ps.ps_ProjInfo = NULL;
    state->ps.ps_ResultTupleDesc = NULL;
    state->ps.instrument = NULL;
    state->ps.needs_to_scan_queue = false;
    state->ps.chgParam = NULL;

    ext->sort_tuples = NULL;
    ext->sort_tupleCount = 0;
    ext->sort_tupleCapacity = 0;
    ext->sort_current = 0;
    ext->sort_finished = false;
    ext->sort_begun = false;
    ext->sort_state = state;  /* 回指供 qsort 比较器使用 */

    (void)eflags;
    return (PlanState *)state;
}

TupleTableSlot *ExecSort(PlanState *pstate) {
    return exec_sort_impl(pstate);
}

void ExecEndSort(SortState *node) {
    if (node == NULL) return;
    /* 释放子节点（generic 递归将经 T_SeqScanState 分派关闭 SeqScan 资源） */
    if (node->ps.lefttree != NULL) {
        ExecEndNode(node->ps.lefttree);
        node->ps.lefttree = NULL;
    }
    /* tuplesortstate / ext / 元组缓冲：均在 es_query_cxt 分配，由 EState 回收 */
    if (node->ps.ps_ExprContext != NULL) {
        FreeExprContext(node->ps.ps_ExprContext, true);
        node->ps.ps_ExprContext = NULL;
    }
    if (node->ps.ps_ResultTupleSlot != NULL) {
        FreeTupleTableSlot(node->ps.ps_ResultTupleSlot);
        node->ps.ps_ResultTupleSlot = NULL;
    }
}

void ExecReScanSort(SortState *node) {
    if (node == NULL) return;
    SortExtState *ext = (SortExtState *)node->ps.plan;
    if (ext != NULL) {
        ext->sort_tuples = NULL;
        ext->sort_tupleCount = 0;
        ext->sort_tupleCapacity = 0;
        ext->sort_current = 0;
        ext->sort_finished = false;
        ext->sort_begun = false;
    }
    if (node->ps.lefttree != NULL) {
        ExecReScan(node->ps.lefttree);
    }
    node->sort_Done = false;
}
