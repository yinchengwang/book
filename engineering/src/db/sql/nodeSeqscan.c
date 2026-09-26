/**
 * @file nodeSeqscan.c
 * @brief SeqScan 执行器节点实现（新 Volcano 阵营，T8 阵营统一）
 *
 * T8 变更要点：
 *   - ExecInitSeqScan 签名统一为新阵营的 (Plan *, EState *, int)——
 *     这是 executor.c 节点注册表（ExecInitNodeFn）实际消费的签名，
 *     消除了旧阵营 (SeqScanPlan *, void *, int) 的跨 TU 签名不一致 UB。
 *   - 旧阵营的 EStatePrefixShim 布局镜像删除：本实现直接使用真实
 *     EState（es_query_cxt），不再依赖"前缀布局恰好一致"的假设。
 *   - 行解码走 sql_row_decode（自描述行格式，见 nodeSeqscan.h），
 *     替代旧实现的固定 2 列 exec_make_tuple_desc 占位。
 *
 * 头文件顺序约定：db/rel.h 必须先于 db/sql/nodeSeqscan.h 引入——
 * rel.h 提供 struct TupleDescData 的真身（attrs[].attname/atttypid），
 * execnodes.h 检测到 DB_REL_H 后跳过占位定义（见 execnodes.h 注释）。
 */

#include <stdio.h>
#include <string.h>

#include "db/rel.h"                 /* 必须先于 executor 头：TupleDescData 真身 */
#include "db/sql/nodeSeqscan.h"     /* -> execnodes.h（检测到 DB_REL_H 跳过占位） */
#include "db/sql/executor.h"        /* MakeTupleTableSlotWithMCxt */

/* 与 nodeModifyTable.h 同款本地保护：parse_node.h 已定义时不重复 */
#ifndef OidIsValid
#define OidIsValid(oid) ((oid) != 0)
#endif

/* PG 类型 OID（行编解码仅支持以下类型，其余显式失败） */
#define SQL_OID_INT2    21
#define SQL_OID_INT4    23
#define SQL_OID_INT8    20
#define SQL_OID_TEXT    25
#define SQL_OID_CHAR    1042
#define SQL_OID_VARCHAR 1043

static bool sql_row_type_supported(Oid t) {
    return t == SQL_OID_INT2 || t == SQL_OID_INT4 || t == SQL_OID_INT8 ||
           t == SQL_OID_TEXT || t == SQL_OID_CHAR || t == SQL_OID_VARCHAR;
}

/* ========================================================================
 * 行编解码
 * 格式：[u32 total_len][per col: u8 isnull + payload]（小端）
 * ======================================================================== */

static void put_u32le(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
    p[2] = (unsigned char)((v >> 16) & 0xff);
    p[3] = (unsigned char)((v >> 24) & 0xff);
}

static uint32_t get_u32le(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

size_t sql_row_encoded_size(int ncols, const Oid *coltypes,
                            const Datum *values, const bool *isnull) {
    size_t total = 4;  /* total_len 前缀 */
    for (int i = 0; i < ncols; i++) {
        if (!sql_row_type_supported(coltypes[i])) {
            return 0;
        }
        total += 1;  /* isnull 标志 */
        if (isnull != NULL && isnull[i]) {
            continue;
        }
        switch (coltypes[i]) {
        case SQL_OID_INT2: total += 2; break;
        case SQL_OID_INT4: total += 4; break;
        case SQL_OID_INT8: total += 8; break;
        default: {  /* text 家族：u32 长度 + 字节 */
            const char *s = (const char *)(uintptr_t)values[i];
            total += 4 + (s != NULL ? strlen(s) : 0);
            break;
        }
        }
    }
    return total;
}

int sql_row_encode(int ncols, const Oid *coltypes,
                   const Datum *values, const bool *isnull,
                   void *buf, size_t cap, size_t *out_len) {
    size_t need = sql_row_encoded_size(ncols, coltypes, values, isnull);
    if (need == 0 || buf == NULL || cap < need) {
        return -1;
    }

    unsigned char *p = (unsigned char *)buf;
    put_u32le(p, (uint32_t)need);
    size_t off = 4;

    for (int i = 0; i < ncols; i++) {
        bool isn = (isnull != NULL && isnull[i]);
        p[off++] = isn ? 1 : 0;
        if (isn) {
            continue;
        }
        switch (coltypes[i]) {
        case SQL_OID_INT2: {
            uint16_t v = (uint16_t)(int64_t)values[i];
            p[off++] = (unsigned char)(v & 0xff);
            p[off++] = (unsigned char)((v >> 8) & 0xff);
            break;
        }
        case SQL_OID_INT4: {
            put_u32le(p + off, (uint32_t)(int64_t)values[i]);
            off += 4;
            break;
        }
        case SQL_OID_INT8: {
            uint64_t v = (uint64_t)values[i];
            for (int b = 0; b < 8; b++) {
                p[off++] = (unsigned char)((v >> (8 * b)) & 0xff);
            }
            break;
        }
        default: {  /* text 家族 */
            const char *s = (const char *)(uintptr_t)values[i];
            size_t len = (s != NULL) ? strlen(s) : 0;
            put_u32le(p + off, (uint32_t)len);
            off += 4;
            if (len > 0) {
                memcpy(p + off, s, len);
                off += len;
            }
            break;
        }
        }
    }

    if (out_len != NULL) {
        *out_len = off;
    }
    return 0;
}

int sql_row_decode(int ncols, const Oid *coltypes,
                   const void *blob,
                   Datum *values, bool *isnull, MemoryContext mcxt) {
    if (blob == NULL || values == NULL || isnull == NULL) {
        return -1;
    }

    const unsigned char *p = (const unsigned char *)blob;
    uint32_t total = get_u32le(p);
    if (total < 4 + (uint32_t)ncols) {
        return -1;  /* 连 isnull 标志位都放不下，必然非法 */
    }

    size_t off = 4;
    for (int i = 0; i < ncols; i++) {
        if (!sql_row_type_supported(coltypes[i])) {
            return -1;
        }
        bool isn = p[off++] != 0;
        isnull[i] = isn;
        values[i] = (Datum)0;
        if (isn) {
            continue;
        }
        switch (coltypes[i]) {
        case SQL_OID_INT2: {
            if (off + 2 > total) return -1;
            int16_t v = (int16_t)((uint16_t)p[off] | ((uint16_t)p[off + 1] << 8));
            values[i] = (Datum)(uint64_t)(int64_t)v;  /* 符号扩展 */
            off += 2;
            break;
        }
        case SQL_OID_INT4: {
            if (off + 4 > total) return -1;
            int32_t v = (int32_t)get_u32le(p + off);
            values[i] = (Datum)(uint64_t)(int64_t)v;
            off += 4;
            break;
        }
        case SQL_OID_INT8: {
            if (off + 8 > total) return -1;
            uint64_t v = 0;
            for (int b = 0; b < 8; b++) {
                v |= ((uint64_t)p[off + b]) << (8 * b);
            }
            values[i] = (Datum)v;
            off += 8;
            break;
        }
        default: {  /* text 家族：u32 长度 + 字节，mcxt 中分配 NUL 结尾副本 */
            if (off + 4 > total) return -1;
            uint32_t len = get_u32le(p + off);
            off += 4;
            if (off + len > total) return -1;
            char *s = (char *)palloc(mcxt, (Size)len + 1);
            if (s == NULL) return -1;
            if (len > 0) {
                memcpy(s, p + off, len);
            }
            s[len] = '\0';
            values[i] = (Datum)(uintptr_t)s;
            off += len;
            break;
        }
        }
    }
    return 0;
}

/* ========================================================================
 * SeqScan 节点
 * ======================================================================== */

PlanState *ExecInitSeqScan(Plan *plan, EState *estate, int eflags) {
    (void)eflags;

    if (plan == NULL || plan->type != T_SeqScan || estate == NULL) {
        return NULL;
    }

    MemoryContext ctx = estate->es_query_cxt;
    if (ctx == NULL) {
        ctx = MemoryContextCurrent();
    }
    if (ctx == NULL) {
        fprintf(stderr, "ExecInitSeqScan: no memory context available\n");
        return NULL;
    }

    SeqScanState *ss = (SeqScanState *)palloc0(ctx, sizeof(SeqScanState));
    if (ss == NULL) {
        return NULL;
    }

    ss->ps.type = T_SeqScanState;
    ss->ps.plan = plan;
    ss->ps.state = estate;
    PlanStateSetExecProc(&ss->ps, ExecSeqScan);

    Oid relid = ((SeqScan *)plan)->scanrelid;
    if (!OidIsValid(relid)) {
        /* scanrelid 无效：无表占位节点。保持 relation 为空，
         * ExecSeqScan 直接返回 NULL（0 行）。 */
        return &ss->ps;
    }

    Relation rel = relation_open(relid, REL_OPEN_READONLY);
    if (rel == NULL) {
        fprintf(stderr, "ExecInitSeqScan: relation_open failed for relid %u\n",
                (unsigned)relid);
        return NULL;  /* ss 在查询上下文中，由 FreeEState 统一回收 */
    }
    ss->ss_currentRelation = rel;

    TupleDesc td = relation_getdesc(rel);
    int natts = (td != NULL) ? td->natts : 0;
    if (td == NULL || natts <= 0) {
        fprintf(stderr, "ExecInitSeqScan: relation %u has no tuple descriptor\n",
                (unsigned)relid);
        relation_close(rel, REL_OPEN_READONLY);
        ss->ss_currentRelation = NULL;
        return NULL;
    }
    ss->ss_natts = natts;

    ss->ss_coltypes = (Oid *)palloc0(ctx, (Size)natts * sizeof(Oid));
    if (ss->ss_coltypes == NULL) {
        relation_close(rel, REL_OPEN_READONLY);
        ss->ss_currentRelation = NULL;
        return NULL;
    }
    for (int i = 0; i < natts; i++) {
        ss->ss_coltypes[i] = td->attrs[i].atttypid;
    }

    TupleTableSlot *slot = MakeTupleTableSlotWithMCxt(ctx);
    if (slot == NULL) {
        relation_close(rel, REL_OPEN_READONLY);
        ss->ss_currentRelation = NULL;
        return NULL;
    }
    slot->tts_tupleDescriptor = td;
    slot->tts_values = (Datum *)palloc0(ctx, (Size)natts * sizeof(Datum));
    slot->tts_isnull = (bool *)palloc0(ctx, (Size)natts * sizeof(bool));
    if (slot->tts_values == NULL || slot->tts_isnull == NULL) {
        relation_close(rel, REL_OPEN_READONLY);
        ss->ss_currentRelation = NULL;
        return NULL;
    }
    slot->tts_nvalid = 0;
    ss->ss_ScanTupleSlot = slot;

    return &ss->ps;
}

TupleTableSlot *ExecSeqScan(PlanState *pstate) {
    if (pstate == NULL || pstate->type != T_SeqScanState) {
        return NULL;
    }
    SeqScanState *ss = (SeqScanState *)pstate;

    if (ss->ss_currentRelation == NULL || ss->ss_ScanTupleSlot == NULL) {
        return NULL;
    }

    /* 惰性开始扫描（Init 时不建描述符，避免 0 行场景白开游标） */
    if (ss->ss_currentScanDesc == NULL) {
        ss->ss_currentScanDesc = table_beginscan(ss->ss_currentRelation, 0, NULL);
        if (ss->ss_currentScanDesc == NULL) {
            fprintf(stderr, "ExecSeqScan: table_beginscan failed\n");
            return NULL;
        }
    }

    void *blob = table_getnext(ss->ss_currentScanDesc);
    if (blob == NULL) {
        return NULL;  /* 扫描结束 */
    }

    MemoryContext ctx = (ss->ps.state != NULL) ? ss->ps.state->es_query_cxt : NULL;
    TupleTableSlot *slot = ss->ss_ScanTupleSlot;
    if (sql_row_decode(ss->ss_natts, ss->ss_coltypes, blob,
                       slot->tts_values, slot->tts_isnull, ctx) != 0) {
        /* 解码失败：显式报告并终止扫描，不静默跳过（行格式由驱动层
         * 独家写入，解码失败即数据损坏或类型越界，"缺行但看似成功"
         * 比显式失败更难排查）。 */
        fprintf(stderr,
                "ExecSeqScan: row decode failed (unsupported type or corrupt row)\n");
        return NULL;
    }
    slot->tts_nvalid = ss->ss_natts;
    return slot;
}

void ExecEndSeqScan(SeqScanState *node) {
    if (node == NULL) {
        return;
    }
    /* 只释放存储层资源；状态结构体内存属于 EState 查询上下文，
     * 由 FreeEState 统一回收（AllocSet 语义，pfree 为空操作）。 */
    if (node->ss_currentScanDesc != NULL) {
        table_endscan(node->ss_currentScanDesc);
        node->ss_currentScanDesc = NULL;
    }
    if (node->ss_currentRelation != NULL) {
        relation_close(node->ss_currentRelation, REL_OPEN_READONLY);
        node->ss_currentRelation = NULL;
    }
}

void ExecReScanSeqScan(SeqScanState *node) {
    if (node == NULL) {
        return;
    }
    if (node->ss_currentScanDesc != NULL) {
        table_endscan(node->ss_currentScanDesc);
        node->ss_currentScanDesc = NULL;
    }
    /* 下次 ExecSeqScan 重新 beginscan */
}
