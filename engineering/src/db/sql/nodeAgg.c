/**
 * @file nodeAgg.c
 * @brief Agg 聚合执行器节点实现（T9：PLAIN + HASHED）
 *
 * PLAIN（无 GROUP BY）：扫空子节点累计聚合，返回单行（0 行输入仍输出
 * 一行：COUNT=0，SUM/MIN/MAX=NULL，与 PG 行为一致）。
 *
 * HASHED（GROUP BY）：扫空子节点按 grpColIdx/grpColTypes 做开放链哈希分桶，
 * 每桶累计聚合；扫空后逐桶输出一行。
 *
 * 子节点返回 NULL = 耗尽；本节点语义完成后必须返回 NULL 而非空 slot。
 *
 * 算子不依赖 TupleDescData 布局：列类型由 Agg.plan.grpColTypes 提供，
 * 输出描述符由驱动在 plan.out_desc 中填好（仅赋值指针，不解引属性）。
 */

#include "db/sql/nodeAgg.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

/* 驱动层 OID 复制（与 sql_driver.c 同步） */
#define AGG_OID_INT2    21
#define AGG_OID_INT4    23
#define AGG_OID_INT8    20
#define AGG_OID_TEXT    25
#define AGG_OID_CHAR    1042
#define AGG_OID_VARCHAR 1043

static inline bool agg_oid_is_text(Oid t) {
    return t == AGG_OID_TEXT || t == AGG_OID_CHAR || t == AGG_OID_VARCHAR;
}

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

typedef struct AggAccSlot {
    bool      seen;          /**< 是否曾收到输入行（控制 SUM/MIN/MAX 的 NULL 输出） */
    int64_t   count;         /**< COUNT 累加（每组/全表） */
    int64_t   sum;           /**< SUM 累加（int 家族 → int64） */
    Datum     min_v;         /**< MIN（Datum 直存；文本指针共享） */
    bool      min_isnull;
    Datum     max_v;
    bool      max_isnull;
} AggAccSlot;

typedef struct AggHashGroup {
    struct AggHashGroup *next;        /**< 链下一组（冲突链） */
    Datum    *keys;                  /**< group key Datums（numCols 项） */
    bool     *keys_isnull;            /**< group key NULL 标记 */
    AggAccSlot *accs;                /**< 与 aggrefs 等长 */
} AggHashGroup;

typedef struct AggHashTable {
    AggHashGroup **buckets;
    int            nbuckets;
    int            ngroups;          /**< 不重复组数 */
    /* 输出阶段用：拉平桶数组为单链表头 */
    AggHashGroup  *group_list;
} AggHashTable;

/* PLAIN 累积器（hash_table 字段指向它） */
typedef struct AggPlainAccum {
    AggAccSlot *accs;
} AggPlainAccum;

/* ========================================================================
 * 辅助：Datum 类型感知比较与哈希
 * ======================================================================== */

static bool agg_datum_eq(Datum a, Datum b, Oid type) {
    if (agg_oid_is_text(type)) {
        const char *sa = a ? (const char *)(uintptr_t)a : "";
        const char *sb = b ? (const char *)(uintptr_t)b : "";
        return strcmp(sa, sb) == 0;
    }
    return (int64_t)a == (int64_t)b;
}

static int agg_datum_cmp(Datum a, Datum b, Oid type) {
    if (agg_oid_is_text(type)) {
        const char *sa = a ? (const char *)(uintptr_t)a : "";
        const char *sb = b ? (const char *)(uintptr_t)b : "";
        return strcmp(sa, sb);
    }
    int64_t la = (int64_t)a, lb = (int64_t)b;
    if (la < lb) return -1;
    if (la > lb) return 1;
    return 0;
}

static uint64_t agg_datum_hash(Datum v, bool isnull, Oid type) {
    /* NULL 与非 NULL 散列区分，避免 NULL 误并入首组 */
    uint64_t h = 1469598103934665603ULL;
    h ^= isnull ? 0xdeadbeefcafe : 0x1234567890ab;
    h *= 1099511628211ULL;
    if (!isnull) {
        if (agg_oid_is_text(type)) {
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

/* ========================================================================
 * 累加子程序：按 AggrefDef 对 (Datum, isnull, arg) 累计到 acc
 * ======================================================================== */

static void agg_accum_one(AggAccSlot *acc, AggrefDef *def,
                          Datum arg, bool arg_null) {
    acc->count++;
    if (def->fn == AGG_FUNC_COUNT) {
        acc->seen = true;  /* T9：COUNT 也视为已见到，至少 1 行 */
        return;  /* COUNT 只看行数 */
    }
    if (arg_null) {
        /* SUM/MIN/MAX 跳过 NULL（PG 默认行为），seen 保持 */
        return;
    }
    if (def->fn == AGG_FUNC_SUM) {
        if (agg_oid_is_text(def->arg_type)) {
            /* T9 不支持文本 SUM：显式拒绝已在驱动层做 */
            return;
        }
        acc->sum += (int64_t)arg;
        acc->seen = true;
        return;
    }
    if (def->fn == AGG_FUNC_MIN) {
        if (acc->min_isnull || agg_datum_cmp(arg, acc->min_v, def->arg_type) < 0) {
            acc->min_v = arg;
            acc->min_isnull = false;
        }
        acc->seen = true;
        return;
    }
    if (def->fn == AGG_FUNC_MAX) {
        if (acc->max_isnull || agg_datum_cmp(arg, acc->max_v, def->arg_type) > 0) {
            acc->max_v = arg;
            acc->max_isnull = false;
        }
        acc->seen = true;
        return;
    }
}

/* ========================================================================
 * 初始化累加器
 * ======================================================================== */

static AggAccSlot *agg_alloc_accs(int n, MemoryContext ctx) {
    AggAccSlot *a = (AggAccSlot *)palloc0(ctx, sizeof(AggAccSlot) * n);
    for (int i = 0; i < n; i++) {
        a[i].min_isnull = true;
        a[i].max_isnull = true;
    }
    return a;
}

/* ========================================================================
 * 哈希分组查找/插入
 * ======================================================================== */

static AggHashGroup *agg_find_or_create_group(AggHashTable *ht,
                                              TupleTableSlot *slot,
                                              int *grp_idx, int num_grp,
                                              Oid *grp_types, int nagg,
                                              MemoryContext ctx) {
    /* 计算哈希 */
    uint64_t h = 1469598103934665603ULL;
    Datum *key_datums = (Datum *)palloc(ctx, sizeof(Datum) * num_grp);
    bool *key_nulls = (bool *)palloc(ctx, sizeof(bool) * num_grp);
    for (int i = 0; i < num_grp; i++) {
        int c = grp_idx[i];
        bool null_v = (slot->tts_isnull == NULL || c >= slot->tts_nvalid)
                      ? true : slot->tts_isnull[c];
        Datum v = (!null_v && slot->tts_values != NULL && c < slot->tts_nvalid)
                  ? slot->tts_values[c] : (Datum)0;
        key_datums[i] = v;
        key_nulls[i] = null_v;
        h = agg_datum_hash(v, null_v, grp_types ? grp_types[i] : AGG_OID_INT4);
    }

    int bucket = (int)(h % (uint64_t)ht->nbuckets);

    /* 查链 */
    for (AggHashGroup *g = ht->buckets[bucket]; g != NULL; g = g->next) {
        bool match = true;
        for (int i = 0; i < num_grp; i++) {
            if (key_nulls[i] != g->keys_isnull[i]) { match = false; break; }
            if (!key_nulls[i] && !agg_datum_eq(key_datums[i], g->keys[i],
                                               grp_types ? grp_types[i] : AGG_OID_INT4)) {
                match = false;
                break;
            }
        }
        if (match) {
            return g;
        }
    }

    /* 未命中：插入新组 */
    AggHashGroup *g = (AggHashGroup *)palloc(ctx, sizeof(AggHashGroup));
    g->keys = (Datum *)palloc(ctx, sizeof(Datum) * num_grp);
    g->keys_isnull = (bool *)palloc(ctx, sizeof(bool) * num_grp);
    memcpy(g->keys, key_datums, sizeof(Datum) * num_grp);
    memcpy(g->keys_isnull, key_nulls, sizeof(bool) * num_grp);
    g->accs = agg_alloc_accs(nagg, ctx);
    g->next = ht->buckets[bucket];
    ht->buckets[bucket] = g;
    ht->ngroups++;
    /* 维护拉平链表（用于输出阶段顺序遍历） */
    g->next = g->next;  /* 占位：实际插入见下 */
    /* 上面 g->next 已设过；这里需要把链头接到 group_list */
    /* 简化：直接 head 插入 group_list */
    return g;
}

/* ========================================================================
 * exec_agg_impl
 * ======================================================================== */

/* 阶段标记：AggState.agg_done 复用为「收集完成」 */
#define PHASE_COLLECT 0
#define PHASE_EMIT    1

static TupleTableSlot *exec_agg_impl(PlanState *pstate) {
    AggState *node = (AggState *)pstate;
    if (node == NULL) return NULL;

    /* ps.plan 字段被劫持为 NULL？不！ps.plan 仍指向原始 plan 节点。T9 扩展
     * 状态挂 hash_table / plain_acc 字段。 */
    int nagg = (node->aggrefs != NULL) ? (int)list_length(node->aggrefs) : 0;

    /* 阶段 1：拉空子节点、累计 */
    if (!node->agg_done) {
        PlanState *child = node->ps.lefttree;
        MemoryContext ctx = ((EState *)node->ps.state)->es_query_cxt;

        /* 初始化聚合状态 */
        if (node->aggstrategy == AGG_PLAIN) {
            if (node->plain_acc == NULL) {
                AggPlainAccum *acc = (AggPlainAccum *)palloc0(ctx, sizeof(AggPlainAccum));
                acc->accs = agg_alloc_accs(nagg, ctx);
                node->plain_acc = acc;
            }
        } else {  /* AGG_HASHED */
            if (node->hash_table == NULL) {
                AggHashTable *ht = (AggHashTable *)palloc0(ctx, sizeof(AggHashTable));
                ht->nbuckets = 256;
                ht->buckets = (AggHashGroup **)palloc0(ctx,
                                    sizeof(AggHashGroup *) * ht->nbuckets);
                ht->ngroups = 0;
                ht->group_list = NULL;
                node->hash_table = ht;
            }
        }

        if (child != NULL) {
            TupleTableSlot *slot;
            while ((slot = ExecProcNode(child)) != NULL) {
                if (node->aggstrategy == AGG_PLAIN) {
                    AggPlainAccum *acc = (AggPlainAccum *)node->plain_acc;
                    ListCell *lc;
                    int i = 0;
                    foreach (lc, node->aggrefs) {
                        AggrefDef *def = (AggrefDef *)lfirst(lc);
                        if (def->fn == AGG_FUNC_COUNT && def->arg_attno < 0) {
                            agg_accum_one(&acc->accs[i], def, (Datum)0, false);
                        } else {
                            int c = def->arg_attno;
                            bool an = (slot->tts_isnull == NULL || c >= slot->tts_nvalid)
                                      ? true : slot->tts_isnull[c];
                            Datum av = (!an && slot->tts_values != NULL && c < slot->tts_nvalid)
                                       ? slot->tts_values[c] : (Datum)0;
                            agg_accum_one(&acc->accs[i], def, av, an);
                        }
                        i++;
                    }
                } else {  /* HASHED */
                    AggHashTable *ht = (AggHashTable *)node->hash_table;
                    AggHashGroup *g = agg_find_or_create_group(
                        ht, slot, node->grpColIdx, node->numCols,
                        node->grpColTypes, nagg, ctx);
                    ListCell *lc;
                    int i = 0;
                    foreach (lc, node->aggrefs) {
                        AggrefDef *def = (AggrefDef *)lfirst(lc);
                        if (def->fn == AGG_FUNC_COUNT && def->arg_attno < 0) {
                            agg_accum_one(&g->accs[i], def, (Datum)0, false);
                        } else {
                            int c = def->arg_attno;
                            bool an = (slot->tts_isnull == NULL || c >= slot->tts_nvalid)
                                      ? true : slot->tts_isnull[c];
                            Datum av = (!an && slot->tts_values != NULL && c < slot->tts_nvalid)
                                       ? slot->tts_values[c] : (Datum)0;
                            agg_accum_one(&g->accs[i], def, av, an);
                        }
                        i++;
                    }
                }
                ExecClearTuple(slot);
            }
        }

        /* 收集完成，构建拉平链表供输出阶段 */
        node->agg_done = true;
        if (node->aggstrategy == AGG_HASHED && node->hash_table != NULL) {
            AggHashTable *ht = (AggHashTable *)node->hash_table;
            /* 把所有桶链头尾相连 → group_list */
            AggHashGroup *head = NULL, *tail = NULL;
            for (int b = 0; b < ht->nbuckets; b++) {
                AggHashGroup *g = ht->buckets[b];
                while (g != NULL) {
                    AggHashGroup *next = g->next;
                    g->next = NULL;
                    if (head == NULL) {
                        head = g;
                    } else {
                        tail->next = g;
                    }
                    tail = g;
                    g = next;
                }
            }
            ht->group_list = head;
        }
        node->hash_ngroups = 0;  /* 输出阶段游标 */
    }

    /* 阶段 2：输出一行 */
    TupleTableSlot *out = node->ps.ps_ResultTupleSlot;
    if (out == NULL || out->tts_values == NULL) {
        return NULL;
    }
    memset(out->tts_isnull, 0, sizeof(bool) * node->out_natts);

    if (node->aggstrategy == AGG_PLAIN) {
        AggPlainAccum *acc = (AggPlainAccum *)node->plain_acc;
        if (node->hash_ngroups > 0) {
            return NULL;  /* PLAIN 仅输出一次 */
        }
        node->hash_ngroups = 1;
        /* group cols 不存在（numCols=0），仅输出聚合 */
        int col = 0;
        ListCell *lc;
        foreach (lc, node->aggrefs) {
            AggrefDef *def = (AggrefDef *)lfirst(lc);
            if (def->fn == AGG_FUNC_COUNT) {
                out->tts_values[col] = (Datum)(int64_t)acc->accs[col].count;
            } else if (def->fn == AGG_FUNC_SUM) {
                if (acc->accs[col].seen) {
                    out->tts_values[col] = (Datum)(int64_t)acc->accs[col].sum;
                } else {
                    out->tts_isnull[col] = true;
                }
            } else {
                if (acc->accs[col].seen) {
                    out->tts_values[col] = (def->fn == AGG_FUNC_MIN)
                                           ? acc->accs[col].min_v
                                           : acc->accs[col].max_v;
                } else {
                    out->tts_isnull[col] = true;
                }
            }
            col++;
        }
        out->tts_nvalid = node->out_natts;
        return out;
    } else {
        /* HASHED：按拉平链表顺序逐组输出 */
        AggHashTable *ht = (AggHashTable *)node->hash_table;
        if (ht == NULL || ht->group_list == NULL) {
            return NULL;  /* 0 行 */
        }
        /* 找到第 hash_ngroups 个组 */
        AggHashGroup *g = ht->group_list;
        for (int i = 0; i < node->hash_ngroups && g != NULL; i++) {
            g = g->next;
        }
        node->hash_ngroups++;
        if (g == NULL) {
            return NULL;
        }
        /* group keys */
        int col = 0;
        for (int i = 0; i < node->numCols; i++) {
            out->tts_values[col] = g->keys[i];
            out->tts_isnull[col] = g->keys_isnull[i];
            col++;
        }
        /* aggs */
        ListCell *lc;
        int i = 0;
        foreach (lc, node->aggrefs) {
            AggrefDef *def = (AggrefDef *)lfirst(lc);
            AggAccSlot *as = &g->accs[i];
            if (def->fn == AGG_FUNC_COUNT) {
                out->tts_values[col] = (Datum)(int64_t)as->count;
                fprintf(stderr, "DEBUG Agg emit COUNT col=%d count=%lld\n",
                        col, (long long)as->count);
            } else if (def->fn == AGG_FUNC_SUM) {
                if (as->seen) {
                    out->tts_values[col] = (Datum)(int64_t)as->sum;
                } else {
                    out->tts_isnull[col] = true;
                }
            } else {
                if (as->seen) {
                    out->tts_values[col] = (def->fn == AGG_FUNC_MIN)
                                           ? as->min_v : as->max_v;
                } else {
                    out->tts_isnull[col] = true;
                }
            }
            col++;
            i++;
        }
        out->tts_nvalid = node->out_natts;
        return out;
    }
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

PlanState *ExecInitAgg(Plan *plan, EState *estate, int eflags) {
    Agg *node;
    AggState *state;
    MemoryContext ctx;

    if (plan == NULL || estate == NULL) return NULL;
    node = (Agg *)plan;
    ctx = estate->es_query_cxt;

    state = (AggState *)palloc0(ctx, sizeof(AggState));
    if (state == NULL) return NULL;

    state->ps.type = T_AggState;
    state->ps.plan = plan;
    state->ps.state = estate;
    state->ps.ExecProcNode = exec_agg_impl;
    state->ps.ExecProcNodeReal = exec_agg_impl;

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

    state->aggstrategy = node->aggstrategy;
    state->numCols = node->numCols;
    state->out_natts = node->out_natts;
    state->out_desc = node->out_desc;
    state->aggrefs = node->aggrefs;

    if (node->numCols > 0 && node->grpColIdx != NULL) {
        state->grpColIdx = (int *)palloc(ctx, sizeof(int) * node->numCols);
        memcpy(state->grpColIdx, node->grpColIdx, sizeof(int) * node->numCols);
    }
    if (node->numCols > 0 && node->grpColTypes != NULL) {
        state->grpColTypes = (Oid *)palloc(ctx, sizeof(Oid) * node->numCols);
        memcpy(state->grpColTypes, node->grpColTypes, sizeof(Oid) * node->numCols);
    }

    /* 分配结果槽 values/isnull 数组 */
    if (state->out_natts > 0) {
        state->ps.ps_ResultTupleSlot->tts_values =
            (Datum *)palloc0(ctx, sizeof(Datum) * state->out_natts);
        state->ps.ps_ResultTupleSlot->tts_isnull =
            (bool *)palloc0(ctx, sizeof(bool) * state->out_natts);
        state->ps.ps_ResultTupleSlot->tts_tupleDescriptor = state->out_desc;
    }

    state->agg_slot = MakeTupleTableSlotWithMCxt(ctx);
    state->agg_done = false;
    state->pergroup = NULL;
    state->hash_table = NULL;
    state->hash_ngroups = 0;
    state->plain_acc = NULL;

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

TupleTableSlot *ExecAgg(PlanState *pstate) {
    return exec_agg_impl(pstate);
}

void ExecEndAgg(AggState *node) {
    if (node == NULL) return;
    if (node->ps.lefttree != NULL) {
        ExecEndNode(node->ps.lefttree);
        node->ps.lefttree = NULL;
    }
    if (node->agg_slot != NULL) {
        FreeTupleTableSlot(node->agg_slot);
        node->agg_slot = NULL;
    }
    if (node->ps.ps_ExprContext != NULL) {
        FreeExprContext(node->ps.ps_ExprContext, true);
        node->ps.ps_ExprContext = NULL;
    }
    if (node->ps.ps_ResultTupleSlot != NULL) {
        FreeTupleTableSlot(node->ps.ps_ResultTupleSlot);
        node->ps.ps_ResultTupleSlot = NULL;
    }
    if (node->grpColIdx != NULL) {
        node->grpColIdx = NULL;
    }
    if (node->pergroup != NULL) {
        node->pergroup = NULL;
    }
    /* hash_table / plain_acc / grpColTypes / aggrefs / out_desc：均在 es_query_cxt 分配 */
}

void ExecReScanAgg(AggState *node) {
    if (node == NULL) return;
    if (node->ps.lefttree != NULL) {
        ExecReScan(node->ps.lefttree);
    }
    node->agg_done = false;
    node->hash_table = NULL;
    node->plain_acc = NULL;
    node->hash_ngroups = 0;
}
