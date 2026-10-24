/**
 * @file nodeIndexscan.c
 * @brief IndexScan 执行器节点实现
 *
 * 实现索引扫描执行器节点，基于 Volcano 迭代器模型。
 * 参考 PostgreSQL 的 nodeIndexscan.c 实现。
 *
 * 注意：IndexScanState 在 sql_executor.h 中已定义，
 * 此处使用 IndexScanExtState 存储扩展状态。
 */

#include "db/sql/nodes/nodeIndexscan.h"

/* 前向声明，避免引入冲突的头文件 */
typedef struct RelationData *Relation;
typedef struct IndexScanDescData *IndexScanDesc;

/* 从 catalog.h 引入 Oid 类型 */
typedef uint32_t Oid;

/* OidIsValid 宏定义 */
#ifndef OidIsValid
#define OidIsValid(oid) ((oid) != 0)
#endif

/* 函数声明（从 rel.h） */
extern Relation relation_open(Oid relid, int mode);
extern void relation_close(Relation rel, int mode);

/* 函数声明（从 btreeam.h） */
extern IndexScanDesc index_beginscan(Relation rel, int nkeys, void *key);
extern IndexScanDesc index_beginscan_heapscan(Relation indexrel, Relation heaprel,
                                               int nkeys, void *key);
extern void index_endscan(IndexScanDesc scan);
extern void *index_getnext(IndexScanDesc scan);

/* RelOpenMode 常量 */
#define RELMODE_READ 0

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * @brief 检查元组是否满足过滤条件
 *
 * @param slot 元组槽
 * @param qual 过滤条件列表
 * @param econtext 表达式上下文
 * @return true 满足条件，false 不满足
 */
static bool ExecQual(TupleTableSlot *slot, List *qual, ExprContext *econtext)
{
    /* 桩实现：暂时无条件通过 */
    (void)slot;
    (void)qual;
    (void)econtext;
    return true;
}

/**
 * @brief 将元组数据复制到元组槽
 *
 * @param slot 目标元组槽
 * @param tuple 源元组数据
 * 
 */
static void ExecCopyTupleToSlot(TupleTableSlot *slot, void *tuple)
{
    if (!slot || !tuple) return;

    /* 清空现有数据 */
    exec_clear_tuple_slot(slot);

    /* 设置元组数据指针（不复制，直接引用） */
    slot->tts_tuple.data = tuple;
    slot->tts_tuple.len = 0;  /* 由描述符决定长度 */
    slot->tts_type = TTS_PHYSICAL;

    /* 标记所有列为有效 */
    if (slot->tts_tupleDescriptor) {
        slot->tts_nvalid = slot->tts_tupleDescriptor->natts;
    }
}

/* ============================================================
 * IndexScan 执行器节点实现
 * ============================================================ */

/**
 * @brief 初始化 IndexScan 执行状态
 */
IndexScanState *ExecInitIndexScan(IndexScanPlan *node, void *estate, int eflags)
{
    IndexScanState *scanstate;
    (void)estate;
    (void)eflags;
    IndexScanExtState *ext_state;

    /* 分配状态结构 */
    scanstate = (IndexScanState *)calloc(1, sizeof(IndexScanState));
    if (!scanstate) {
        return NULL;
    }

    /* 分配扩展状态结构 */
    ext_state = (IndexScanExtState *)calloc(1, sizeof(IndexScanExtState));
    if (!ext_state) {
        free(scanstate);
        return NULL;
    }

    /* 初始化基类 */
    scanstate->ss.ps.type = EXEC_INDEX_SCAN;
    scanstate->ss.ps.left = NULL;
    scanstate->ss.ps.right = NULL;
    scanstate->ss.ps.state = ext_state;  /* 通过 state 指针关联扩展状态 */

    /* 设置执行函数指针 */
    scanstate->ss.ps.exec_proc = (TupleTableSlot *(*)(PlanState *))exec_index_scan;

    /* 复制计划和目标列表 */
    ext_state->iss_targetlist = node->targetlist;
    ext_state->iss_qual = node->qual;
    ext_state->iss_indexqual = NULL;  /* 从 node->key 提取，暂不实现 */

    /* 初始化扫描信息 */
    ext_state->iss_indexRelation = NULL;
    ext_state->iss_heapRelation = NULL;
    ext_state->iss_ScanDesc = NULL;

    /* 初始化统计信息 */
    ext_state->iss_tuples_scanned = 0;
    ext_state->iss_tuples_returned = 0;
    ext_state->iss_index_entries = 0;

    /* 初始化状态标志 */
    ext_state->iss_scan_started = false;
    ext_state->iss_scan_ended = false;

    /* 创建元组描述符（从目标列表推断） */
    /* 暂时使用固定 2 列作为测试 */
    scanstate->ss.ps.ps_TupDesc = exec_make_tuple_desc(2);
    if (!scanstate->ss.ps.ps_TupDesc) {
        free(ext_state);
        free(scanstate);
        return NULL;
    }

    /* 创建表达式上下文 */
    scanstate->ss.ps.expr_context = exec_create_expr_context();
    if (!scanstate->ss.ps.expr_context) {
        exec_drop_tuple_desc(scanstate->ss.ps.ps_TupDesc);
        free(ext_state);
        free(scanstate);
        return NULL;
    }

    /* 创建元组槽 */
    scanstate->ss.ps.expr_context->slot = exec_make_tuple_slot(scanstate->ss.ps.ps_TupDesc);
    if (!scanstate->ss.ps.expr_context->slot) {
        exec_destroy_expr_context(scanstate->ss.ps.expr_context);
        exec_drop_tuple_desc(scanstate->ss.ps.ps_TupDesc);
        free(ext_state);
        free(scanstate);
        return NULL;
    }

    /* 如果提供了 indexid 和 relid，初始化 Relation */
    if (OidIsValid(node->indexid) && OidIsValid(node->relid)) {
        if (ExecInitIndexScanRelation(scanstate, ext_state,
                                       node->indexid, node->relid) != 0) {
            exec_drop_tuple_slot(scanstate->ss.ps.expr_context->slot);
            exec_destroy_expr_context(scanstate->ss.ps.expr_context);
            exec_drop_tuple_desc(scanstate->ss.ps.ps_TupDesc);
            free(ext_state);
            free(scanstate);
            return NULL;
        }
    }

    return scanstate;
}

/**
 * @brief 执行 IndexScan 迭代
 */
TupleTableSlot *exec_index_scan(IndexScanState *node)
{
    IndexScanExtState *ext_state;
    TupleTableSlot *slot;
    void *tuple;
    ExprContext *econtext;

    /* 检查参数 */
    if (!node) return NULL;

    /* 获取扩展状态 */
    ext_state = (IndexScanExtState *)node->ss.ps.state;
    if (!ext_state) return NULL;

    /* 获取表达式上下文 */
    econtext = node->ss.ps.expr_context;
    slot = econtext ? econtext->slot : NULL;
    if (!slot) return NULL;

    /* 检查是否已结束扫描 */
    if (ext_state->iss_scan_ended) {
        return NULL;
    }

    /* 如果还没有开始扫描，初始化扫描 */
    if (!ext_state->iss_scan_started) {
        if (ext_state->iss_indexRelation && ext_state->iss_heapRelation) {
            /* 开始索引扫描 */
            ext_state->iss_ScanDesc = index_beginscan_heapscan(
                ext_state->iss_indexRelation,
                ext_state->iss_heapRelation,
                0,  /* nkeys */
                NULL /* scankey */
            );
        }
        ext_state->iss_scan_started = true;
    }

    /* Volcano 模型：迭代拉取元组 */
    while (true) {
        /* 获取下一个索引条目对应的堆元组 */
        if (ext_state->iss_ScanDesc) {
            tuple = index_getnext(ext_state->iss_ScanDesc);
        } else {
            /* 没有 Relation，返回 NULL */
            tuple = NULL;
        }

        /* 检查是否到达末尾 */
        if (!tuple) {
            ext_state->iss_scan_ended = true;
            return NULL;
        }

        /* 更新统计信息 */
        ext_state->iss_tuples_scanned++;

        /* 将元组复制到槽中 */
        ExecCopyTupleToSlot(slot, tuple);

        /* 设置表达式上下文 */
        if (econtext) {
            econtext->slot = slot;
        }

        /* 检查过滤条件（非索引条件） */
        if (ExecQual(slot, ext_state->iss_qual, econtext)) {
            ext_state->iss_tuples_returned++;
            return slot;
        }

        /* 不满足条件，继续下一个元组 */
    }
}

/**
 * @brief 结束 IndexScan 执行
 */
void ExecEndIndexScan(IndexScanState *node)
{
    IndexScanExtState *ext_state;

    if (!node) return;

    ext_state = (IndexScanExtState *)node->ss.ps.state;

    /* 结束索引扫描 */
    if (ext_state && ext_state->iss_ScanDesc) {
        index_endscan(ext_state->iss_ScanDesc);
        ext_state->iss_ScanDesc = NULL;
    }

    /* 关闭索引 Relation */
    if (ext_state && ext_state->iss_indexRelation) {
        relation_close(ext_state->iss_indexRelation, RELMODE_READ);
        ext_state->iss_indexRelation = NULL;
    }

    /* 关闭堆 Relation */
    if (ext_state && ext_state->iss_heapRelation) {
        relation_close(ext_state->iss_heapRelation, RELMODE_READ);
        ext_state->iss_heapRelation = NULL;
    }

    /* 释放表达式上下文 */
    if (node->ss.ps.expr_context) {
        if (node->ss.ps.expr_context->slot) {
            exec_drop_tuple_slot(node->ss.ps.expr_context->slot);
        }
        exec_destroy_expr_context(node->ss.ps.expr_context);
        node->ss.ps.expr_context = NULL;
    }

    /* 释放元组描述符 */
    if (node->ss.ps.ps_TupDesc) {
        exec_drop_tuple_desc(node->ss.ps.ps_TupDesc);
        node->ss.ps.ps_TupDesc = NULL;
    }

    /* 释放扩展状态 */
    if (ext_state) {
        free(ext_state);
        node->ss.ps.state = NULL;
    }
}

/**
 * @brief 重置 IndexScan 执行状态
 */
void ExecReScanIndexScan(IndexScanState *node)
{
    IndexScanExtState *ext_state;

    if (!node) return;

    ext_state = (IndexScanExtState *)node->ss.ps.state;
    if (!ext_state) return;

    /* 重置统计信息 */
    ext_state->iss_tuples_scanned = 0;
    ext_state->iss_tuples_returned = 0;

    /* 重置状态标志 */
    ext_state->iss_scan_started = false;
    ext_state->iss_scan_ended = false;

    /* 如果已有扫描描述符，重置它 */
    if (ext_state->iss_ScanDesc) {
        index_endscan(ext_state->iss_ScanDesc);
        ext_state->iss_ScanDesc = NULL;
    }
}

/**
 * @brief 初始化 IndexScan 的 Relation
 */
int ExecInitIndexScanRelation(IndexScanState *node, IndexScanExtState *ext_state,
                              Oid indexid, Oid relid)
{
    Relation indexrel;
    Relation heaprel;

    if (!node || !ext_state || !OidIsValid(indexid) || !OidIsValid(relid)) {
        return -1;
    }

    /* 打开索引 Relation */
    indexrel = relation_open(indexid, RELMODE_READ);
    if (!indexrel) {
        return -1;
    }

    /* 打开堆 Relation */
    heaprel = relation_open(relid, RELMODE_READ);
    if (!heaprel) {
        relation_close(indexrel, RELMODE_READ);
        return -1;
    }

    ext_state->iss_indexRelation = indexrel;
    ext_state->iss_heapRelation = heaprel;

    return 0;
}

/**
 * @brief 获取 IndexScan 的扩展状态
 */
IndexScanExtState *ExecGetIndexScanExtState(IndexScanState *node)
{
    if (!node) return NULL;
    return (IndexScanExtState *)node->ss.ps.state;
}

/**
 * @brief 获取 IndexScan 的元组描述符
 */
ExecTupleDesc *ExecGetIndexScanTupleDesc(IndexScanState *node)
{
    if (!node) return NULL;
    return node->ss.ps.ps_TupDesc;
}

/**
 * @brief 构造 IndexScan 计划节点
 */
IndexScanPlan *make_indexscan_plan(Oid indexid, Oid relid, int nkeys,
                                   struct ScanKeyData *key)
{
    IndexScanPlan *node;

    node = (IndexScanPlan *)calloc(1, sizeof(IndexScanPlan));
    if (!node) return NULL;

    node->type = EXEC_INDEX_SCAN;
    node->indexid = indexid;
    node->relid = relid;
    node->nkeys = nkeys;
    node->key = key;
    node->qual = NULL;
    node->targetlist = NULL;
    node->indexorderby = NULL;

    return node;
}