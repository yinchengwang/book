/**
 * @file nodeHashjoin.c
 * @brief HashJoin 等值连接执行器节点实现（T9）
 *
 * Volcano 两阶段：
 *   1. 构建阶段（hj_built=false）：拉空内表（righttree），按 inner_key 哈希
 *      到 bucket 链（深拷贝 slot 值）。本实现取 clauses[0] 作为唯一连接键
 *      （满足 T9 测试场景：ON a.id = b.id）。
 *   2. 探测阶段（hj_built=true）：拉取外表（lefttree）一行，逐桶链比较，
 *      命中则把 outer cols ++ inner cols 填入 ps_ResultTupleSlot 返回。
 *      多匹配：外表行缓存于 hj_CurOuterSlot，probe_chain_idx 记录当前
 *      bucket 位置，匹配耗尽后丢弃外表行继续拉下一行。
 *
 * 算子不依赖 TupleDescData 布局：连接键类型由 HJClause.key_type 提供，
 * 输出描述符由驱动在 plan.out_desc 中填好（仅赋值指针，不解引属性）。
 */

#include "db/sql/nodeHashjoin.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* 驱动层 OID 复制（与 sql_driver.c / nodeAgg.c 同步） */
#define HJ_OID_INT2    21
#define HJ_OID_INT4    23
#define HJ_OID_INT8    20
#define HJ_OID_TEXT    25
#define HJ_OID_CHAR    1042
#define HJ_OID_VARCHAR 1043

static inline bool hj_oid_is_text(Oid t) {
    return t == HJ_OID_TEXT || t == HJ_OID_CHAR || t == HJ_OID_VARCHAR;
}

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/** 单条内表行（按 inner_key 哈希落入桶链）。深拷贝 slot 值。 */
typedef struct HashJoinBucket {
    struct HashJoinBucket *next;     /**< 同桶链下一项（冲突链） */
    Datum                  key;       /**< inner 键值 */
    bool                   key_null;
    /* 整行 inner slot 内容（仅 tts_values/tts_isnull，按 nvalid 大小） */
    Datum                 *values;
    bool                  *isnull;
    int                    nvalid;
} HashJoinBucket;

typedef struct HashJoinHashTable {
    HashJoinBucket **buckets;
    int              nbuckets;
    int              ninner;         /**< 内表总行数 */
} HashJoinHashTable;

/* ========================================================================
 * 辅助：Datum 类型感知哈希与等值
 * ======================================================================== */

static uint64_t hj_hash(Datum v, bool isnull, Oid type) {
    uint64_t h = 1469598103934665603ULL;
    h ^= isnull ? 0xdeadbeefcafe : 0x1234567890ab;
    h *= 1099511628211ULL;
    if (!isnull) {
        if (hj_oid_is_text(type)) {
            const char *s = (const char *)(uintptr_t)v;
            while (*s) {
                h ^= (uint8_t)*s++;
                h *= 1099511628211ULL;
            }
        } else {
            uint64_t v64 = (uint64_t)(int64_t)v;
            for (int i = 0; i < 8; i++) {
                h ^= (v64 >> (i * 8)) & 0xff;
                h *= 1099511628211ULL;
            }
        }
    }
    return h;
}

static bool hj_eq(Datum a, Datum b, bool na, bool nb, Oid type) {
    if (na != nb) return false;
    if (na) return true;  /* 双 NULL → 等值 */
    if (hj_oid_is_text(type)) {
        const char *sa = (const char *)(uintptr_t)a;
        const char *sb = (const char *)(uintptr_t)b;
        return strcmp(sa, sb) == 0;
    }
    return (int64_t)a == (int64_t)b;
}

/* 取 slot 上指定列的 Datum + isnull。安全护栏。 */
static Datum hj_slot_get(TupleTableSlot *slot, int attno,
                         bool *out_isnull) {
    if (slot == NULL || slot->tts_values == NULL ||
        slot->tts_isnull == NULL || attno >= slot->tts_nvalid) {
        if (out_isnull) *out_isnull = true;
        return (Datum)0;
    }
    if (out_isnull) *out_isnull = slot->tts_isnull[attno];
    return slot->tts_isnull[attno] ? (Datum)0 : slot->tts_values[attno];
}

/* ========================================================================
 * 构建阶段
 * ======================================================================== */

static void hj_build_phase(HashJoinState *node, EState *estate) {
    MemoryContext ctx = estate->es_query_cxt;
    PlanState *inner = node->js.ps.righttree;
    HJClause *cl = node->nclauses > 0 ? &node->clauses[0] : NULL;

    HashJoinHashTable *ht = (HashJoinHashTable *)palloc0(ctx,
                                                       sizeof(HashJoinHashTable));
    ht->nbuckets = 256;
    ht->buckets = (HashJoinBucket **)palloc0(ctx,
                            sizeof(HashJoinBucket *) * ht->nbuckets);
    ht->ninner = 0;
    node->hashtable = ht;

    if (inner == NULL || cl == NULL) {
        node->hj_built = true;
        return;
    }

    Oid key_type = cl->key_type;
    int  inner_attno = cl->inner_attno;

    TupleTableSlot *slot;
    while ((slot = ExecProcNode(inner)) != NULL) {
        bool isnull;
        Datum key = hj_slot_get(slot, inner_attno, &isnull);
        uint64_t h = hj_hash(key, isnull, key_type);
        int b = (int)(h % (uint64_t)ht->nbuckets);

        HashJoinBucket *bk = (HashJoinBucket *)palloc0(ctx,
                                                     sizeof(HashJoinBucket));
        bk->next = ht->buckets[b];
        bk->key = key;
        bk->key_null = isnull;
        int nv = slot->tts_nvalid > 0 ? slot->tts_nvalid : 0;
        bk->nvalid = nv;
        bk->values = (Datum *)palloc(ctx, sizeof(Datum) * nv);
        bk->isnull = (bool *)palloc(ctx, sizeof(bool) * nv);
        if (slot->tts_values != NULL && nv > 0) memcpy(bk->values, slot->tts_values,
                                             sizeof(Datum) * nv);
        if (slot->tts_isnull != NULL && nv > 0) memcpy(bk->isnull, slot->tts_isnull,
                                             sizeof(bool) * nv);
        ht->buckets[b] = bk;
        ht->ninner++;
        ExecClearTuple(slot);
    }

    node->hj_built = true;
}

/* ========================================================================
 * exec_hashjoin_impl
 * ======================================================================== */

static TupleTableSlot *exec_hashjoin_impl(PlanState *pstate) {
    HashJoinState *node = (HashJoinState *)pstate;
    if (node == NULL) return NULL;

    EState *estate = (EState *)node->js.ps.state;
    MemoryContext ctx = estate->es_query_cxt;
    TupleTableSlot *out = node->js.ps.ps_ResultTupleSlot;
    if (out == NULL || out->tts_values == NULL) return NULL;

    /* Build phase（一次性） */
    if (!node->hj_built) {
        hj_build_phase(node, estate);
    }

    HJClause *cl = node->nclauses > 0 ? &node->clauses[0] : NULL;
    HashJoinHashTable *ht = (HashJoinHashTable *)node->hashtable;

    /* 探测循环：每次返回一行 */
    while (true) {
        /* 若当前已有外表行（多匹配模式），在同桶链上继续推进 */
        if (node->hj_outer_active && cl != NULL && ht != NULL) {
            HashJoinBucket *b = node->probe_chain;
            int idx = node->probe_chain_idx;
            for (int i = 0; i < idx && b != NULL; i++) {
                b = b->next;
            }
            if (b == NULL) {
                /* 链耗尽，进入下一行 */
                node->hj_outer_active = false;
                node->probe_chain = NULL;
                node->probe_chain_idx = 0;
                continue;
            }
            /* 检查类型感知相等 */
            bool match = hj_eq(node->hj_CurOuterSlot
                                ? node->hj_CurOuterSlot->tts_values[cl->outer_attno]
                                : (Datum)0,
                               b->key,
                               node->hj_CurOuterSlot
                                ? node->hj_CurOuterSlot->tts_isnull[cl->outer_attno]
                                : true,
                               b->key_null,
                               cl->key_type);
            node->probe_chain_idx++;
            if (!match) {
                continue;  /* 同一外表行试下一桶 */
            }
            /* 命中 → 组装 outer ++ inner 到 out */
            TupleTableSlot *outer = node->hj_CurOuterSlot;
            int on = node->outer_natts;
            memset(out->tts_isnull, 0, sizeof(bool) * node->out_natts);
            for (int i = 0; i < on; i++) {
                if (outer == NULL || outer->tts_values == NULL ||
                    outer->tts_isnull == NULL || i >= outer->tts_nvalid) {
                    out->tts_isnull[i] = true;
                    out->tts_values[i] = (Datum)0;
                } else {
                    out->tts_isnull[i] = outer->tts_isnull[i];
                    out->tts_values[i] = outer->tts_values[i];
                }
            }
            for (int i = 0; i < b->nvalid; i++) {
                int col = on + i;
                if (col >= node->out_natts) break;
                out->tts_isnull[col] = b->isnull[i];
                out->tts_values[col] = b->values[i];
            }
            out->tts_nvalid = node->out_natts;
            return out;
        }

        /* 没有活跃外表行：拉新行 */
        PlanState *outer_ps = node->js.ps.lefttree;
        if (outer_ps == NULL || cl == NULL || ht == NULL) {
            return NULL;
        }
        TupleTableSlot *outer = ExecProcNode(outer_ps);
        if (outer == NULL) {
            return NULL;  /* 外表耗尽 */
        }
        bool o_isnull;
        Datum o_key = hj_slot_get(outer, cl->outer_attno, &o_isnull);
        uint64_t h = hj_hash(o_key, o_isnull, cl->key_type);
        int b = (int)(h % (uint64_t)ht->nbuckets);

        /* 缓存外表行（深拷贝以保证后续多匹配期间 outer slot 不被覆盖） */
        if (node->hj_CurOuterSlot == NULL) {
            node->hj_CurOuterSlot = MakeTupleTableSlotWithMCxt(ctx);
        }
        TupleTableSlot *cs = node->hj_CurOuterSlot;
        int on = node->outer_natts;
        if (cs->tts_values == NULL || cs->tts_nvalid < on) {
            cs->tts_values = (Datum *)palloc(ctx, sizeof(Datum) * on);
            cs->tts_isnull = (bool *)palloc(ctx, sizeof(bool) * on);
            cs->tts_nvalid = on;
        }
        for (int i = 0; i < on; i++) {
            if (outer->tts_values == NULL || outer->tts_isnull == NULL ||
                i >= outer->tts_nvalid) {
                cs->tts_isnull[i] = true;
                cs->tts_values[i] = (Datum)0;
            } else {
                cs->tts_isnull[i] = outer->tts_isnull[i];
                cs->tts_values[i] = outer->tts_values[i];
            }
        }
        cs->tts_tupleDescriptor = outer->tts_tupleDescriptor;
        ExecClearTuple(outer);

        node->hj_CurOuterSlot = cs;
        node->hj_outer_active = true;
        node->probe_chain = ht->buckets[b];
        node->probe_chain_idx = 0;
        /* 循环回顶部，从 hj_outer_active 分支处理第一个匹配 */
    }
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

PlanState *ExecInitHashJoin(Plan *plan, EState *estate, int eflags) {
    HashJoin *node;
    HashJoinState *state;
    MemoryContext ctx;

    if (plan == NULL || estate == NULL) return NULL;
    node = (HashJoin *)plan;
    ctx = estate->es_query_cxt;

    state = (HashJoinState *)palloc0(ctx, sizeof(HashJoinState));
    if (state == NULL) return NULL;

    state->js.ps.type = T_HashJoinState;
    state->js.ps.plan = plan;
    state->js.ps.state = estate;
    state->js.ps.ExecProcNode = exec_hashjoin_impl;
    state->js.ps.ExecProcNodeReal = exec_hashjoin_impl;

    /* lefttree = outer, righttree = inner */
    if (node->join.plan.lefttree != NULL) {
        state->js.ps.lefttree = ExecInitNode(node->join.plan.lefttree,
                                             estate, eflags);
    } else {
        state->js.ps.lefttree = NULL;
    }
    if (node->join.plan.righttree != NULL) {
        state->js.ps.righttree = ExecInitNode(node->join.plan.righttree,
                                              estate, eflags);
    } else {
        state->js.ps.righttree = NULL;
    }

    state->js.joinqual = NULL;
    state->js.ps.ps_ExprContext = CreateExprContext(estate);
    if (state->js.ps.ps_ExprContext == NULL) return NULL;

    state->js.ps.ps_ResultTupleSlot = MakeTupleTableSlotWithMCxt(ctx);
    if (state->js.ps.ps_ResultTupleSlot == NULL) return NULL;

    /* 复制 HJClause 列表（List 元素按 (HJClause*) 装入） */
    int nc = (node->hashclauses != NULL)
             ? (int)list_length(node->hashclauses) : 0;
    state->clauses = NULL;
    state->nclauses = 0;
    if (nc > 0) {
        state->clauses = (HJClause *)palloc(ctx, sizeof(HJClause) * nc);
        ListCell *lc;
        int i = 0;
        foreach (lc, node->hashclauses) {
            HJClause *src = (HJClause *)lfirst(lc);
            state->clauses[i] = *src;
            i++;
        }
        state->nclauses = nc;
    }

    state->out_desc = node->out_desc;
    state->out_natts = node->out_natts;
    state->outer_natts = node->outer_natts;
    state->inner_natts = node->inner_natts;

    /* 分配结果槽 values/isnull 数组 */
    if (state->out_natts > 0) {
        state->js.ps.ps_ResultTupleSlot->tts_values =
            (Datum *)palloc0(ctx, sizeof(Datum) * state->out_natts);
        state->js.ps.ps_ResultTupleSlot->tts_isnull =
            (bool *)palloc0(ctx, sizeof(bool) * state->out_natts);
        state->js.ps.ps_ResultTupleSlot->tts_tupleDescriptor = state->out_desc;
    }

    state->hashtable = NULL;
    state->hj_OuterTupleSlot = NULL;
    state->hj_InnerTupleSlot = NULL;
    state->hj_NullInnerTupleSlot = NULL;
    state->hj_FirstOuterTupleSlot = false;
    state->hj_CurOuterNoMatch = 0;
    state->probe_chain = NULL;
    state->probe_chain_idx = 0;
    state->hj_CurOuterSlot = NULL;
    state->hj_built = false;
    state->hj_outer_active = false;

    state->js.ps.qual = NULL;
    state->js.ps.recheck = NULL;
    state->js.ps.ps_ProjInfo = NULL;
    state->js.ps.ps_ResultTupleDesc = NULL;
    state->js.ps.instrument = NULL;
    state->js.ps.needs_to_scan_queue = false;
    state->js.ps.chgParam = NULL;

    (void)eflags;
    return (PlanState *)state;
}

TupleTableSlot *ExecHashJoin(PlanState *pstate) {
    return exec_hashjoin_impl(pstate);
}

void ExecEndHashJoin(HashJoinState *node) {
    if (node == NULL) return;
    if (node->js.ps.lefttree != NULL) {
        ExecEndNode(node->js.ps.lefttree);
        node->js.ps.lefttree = NULL;
    }
    if (node->js.ps.righttree != NULL) {
        ExecEndNode(node->js.ps.righttree);
        node->js.ps.righttree = NULL;
    }
    if (node->hj_OuterTupleSlot != NULL) {
        FreeTupleTableSlot(node->hj_OuterTupleSlot);
        node->hj_OuterTupleSlot = NULL;
    }
    if (node->hj_InnerTupleSlot != NULL) {
        FreeTupleTableSlot(node->hj_InnerTupleSlot);
        node->hj_InnerTupleSlot = NULL;
    }
    if (node->hj_NullInnerTupleSlot != NULL) {
        FreeTupleTableSlot(node->hj_NullInnerTupleSlot);
        node->hj_NullInnerTupleSlot = NULL;
    }
    if (node->hj_CurOuterSlot != NULL) {
        FreeTupleTableSlot(node->hj_CurOuterSlot);
        node->hj_CurOuterSlot = NULL;
    }
    if (node->js.ps.ps_ExprContext != NULL) {
        FreeExprContext(node->js.ps.ps_ExprContext, true);
        node->js.ps.ps_ExprContext = NULL;
    }
    if (node->js.ps.ps_ResultTupleSlot != NULL) {
        FreeTupleTableSlot(node->js.ps.ps_ResultTupleSlot);
        node->js.ps.ps_ResultTupleSlot = NULL;
    }
    /* hashtable / clauses / out_desc：均在 es_query_cxt 分配 */
}

void ExecReScanHashJoin(HashJoinState *node) {
    if (node == NULL) return;
    if (node->js.ps.lefttree != NULL) {
        ExecReScan(node->js.ps.lefttree);
    }
    if (node->js.ps.righttree != NULL) {
        ExecReScan(node->js.ps.righttree);
    }
    node->hashtable = NULL;
    node->hj_built = false;
    node->hj_outer_active = false;
    node->probe_chain = NULL;
    node->probe_chain_idx = 0;
}