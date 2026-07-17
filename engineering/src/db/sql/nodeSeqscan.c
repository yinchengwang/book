/**
 * @file nodeSeqscan.c
 * @brief SeqScan 执行器节点实现
 *
 * 实现顺序扫描执行器节点，基于 Volcano 迭代器模型。
 * 参考 PostgreSQL 的 nodeSeqscan.c 实现。
 *
 * 注意：SeqScanState 在 sql_executor.h 中已定义，
 * 此处使用 SeqScanExtState 存储扩展状态。
 */

#include "db/sql/nodes/nodeSeqscan.h"

/* 前向声明，避免引入冲突的头文件 */
typedef struct RelationData *Relation;
typedef struct TableScanDescData *TableScanDesc;

/* 从 catalog.h 引入 Oid 类型 */
typedef uint32_t Oid;

/* OidIsValid 宏定义 */
#ifndef OidIsValid
#define OidIsValid(oid) ((oid) != 0)
#endif

/* 函数声明（从 rel.h） */
extern Relation relation_open(Oid relid, int mode);
extern void relation_close(Relation rel, int mode);
extern TableScanDesc table_beginscan(Relation rel, int nkeys, void *key);
extern void table_endscan(TableScanDesc scan);
extern void *table_getnext(TableScanDesc scan);

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
 * @param desc 元组描述符
 */
static void ExecCopyTupleToSlot(TupleTableSlot *slot, void *tuple, TupleDesc desc)
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

    (void)desc;  /* 未使用 */
}

/* ============================================================
 * SeqScan 执行器节点实现
 * ============================================================ */

/**
 * @brief 初始化 SeqScan 执行状态
 */
SeqScanState *ExecInitSeqScan(SeqScanPlan *node, void *estate, int eflags)
{
    SeqScanState *scanstate;
    SeqScanExtState *ext_state;

    /* 分配状态结构 */
    scanstate = (SeqScanState *)calloc(1, sizeof(SeqScanState));
    if (!scanstate) {
        return NULL;
    }

    /* 分配扩展状态结构 */
    ext_state = (SeqScanExtState *)calloc(1, sizeof(SeqScanExtState));
    if (!ext_state) {
        free(scanstate);
        return NULL;
    }

    /* 初始化基类 */
    scanstate->ss.ps.type = EXEC_SEQ_SCAN;
    scanstate->ss.ps.left = NULL;
    scanstate->ss.ps.right = NULL;
    scanstate->ss.ps.state = ext_state;  /* 通过 state 指针关联扩展状态 */

    /* 设置执行函数指针 */
    scanstate->ss.ps.exec_proc = ExecSeqScan;

    /* 复制计划和目标列表 */
    ext_state->ss_targetlist = node->targetlist;
    ext_state->ss_qual = node->qual;

    /* 初始化扫描信息 */
    ext_state->ss_currentRelation = NULL;
    ext_state->ss_currentScanDesc = NULL;

    /* 初始化统计信息 */
    ext_state->ss_tuples_scanned = 0;
    ext_state->ss_tuples_returned = 0;

    /* 初始化状态标志 */
    ext_state->ss_scan_started = false;
    ext_state->ss_scan_ended = false;

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

    /* 如果提供了 scanrelid，初始化 Relation */
    if (OidIsValid(node->scanrelid)) {
        if (ExecInitSeqScanRelation(scanstate, ext_state, node->scanrelid) != 0) {
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
 * @brief 执行 SeqScan 迭代
 */
TupleTableSlot *ExecSeqScan(PlanState *pstate)
{
    SeqScanState *node = (SeqScanState *)pstate;
    SeqScanExtState *ext_state;
    TupleTableSlot *slot;
    void *tuple;
    ExprContext *econtext;

    /* 检查参数 */
    if (!node) return NULL;

    /* 获取扩展状态 */
    ext_state = (SeqScanExtState *)node->ss.ps.state;
    if (!ext_state) return NULL;

    /* 获取表达式上下文 */
    econtext = node->ss.ps.expr_context;
    slot = econtext ? econtext->slot : NULL;
    if (!slot) return NULL;

    /* 检查是否已结束扫描 */
    if (ext_state->ss_scan_ended) {
        return NULL;
    }

    /* 如果还没有开始扫描，初始化扫描 */
    if (!ext_state->ss_scan_started) {
        if (ext_state->ss_currentRelation && !ext_state->ss_currentScanDesc) {
            /* 开始表扫描 */
            ext_state->ss_currentScanDesc = table_beginscan(
                ext_state->ss_currentRelation,
                0,  /* nkeys */
                NULL /* scankey */
            );
        }
        ext_state->ss_scan_started = true;
    }

    /* Volcano 模型：迭代拉取元组 */
    while (true) {
        /* 获取下一个元组 */
        if (ext_state->ss_currentScanDesc) {
            tuple = table_getnext(ext_state->ss_currentScanDesc);
        } else {
            /* 没有 Relation，返回 NULL */
            tuple = NULL;
        }

        /* 检查是否到达末尾 */
        if (!tuple) {
            ext_state->ss_scan_ended = true;
            return NULL;
        }

        /* 更新统计信息 */
        ext_state->ss_tuples_scanned++;

        /* 将元组复制到槽中（不使用 Relation 内部字段） */
        ExecCopyTupleToSlot(slot, tuple, NULL);

        /* 设置表达式上下文 */
        if (econtext) {
            econtext->slot = slot;
        }

        /* 检查过滤条件 */
        if (ExecQual(slot, ext_state->ss_qual, econtext)) {
            ext_state->ss_tuples_returned++;
            return slot;
        }

        /* 不满足条件，继续下一个元组 */
    }
}

/**
 * @brief 结束 SeqScan 执行
 */
void ExecEndSeqScan(SeqScanState *node)
{
    SeqScanExtState *ext_state;

    if (!node) return;

    ext_state = (SeqScanExtState *)node->ss.ps.state;

    /* 结束表扫描 */
    if (ext_state && ext_state->ss_currentScanDesc) {
        table_endscan(ext_state->ss_currentScanDesc);
        ext_state->ss_currentScanDesc = NULL;
    }

    /* 关闭 Relation */
    if (ext_state && ext_state->ss_currentRelation) {
        relation_close(ext_state->ss_currentRelation, RELMODE_READ);
        ext_state->ss_currentRelation = NULL;
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
 * @brief 重置 SeqScan 执行状态
 */
void ExecReScanSeqScan(SeqScanState *node)
{
    SeqScanExtState *ext_state;

    if (!node) return;

    ext_state = (SeqScanExtState *)node->ss.ps.state;
    if (!ext_state) return;

    /* 重置统计信息 */
    ext_state->ss_tuples_scanned = 0;
    ext_state->ss_tuples_returned = 0;

    /* 重置状态标志 */
    ext_state->ss_scan_started = false;
    ext_state->ss_scan_ended = false;

    /* 如果已有扫描描述符，重置它 */
    if (ext_state->ss_currentScanDesc) {
        table_endscan(ext_state->ss_currentScanDesc);
        ext_state->ss_currentScanDesc = NULL;
    }
}

/**
 * @brief 初始化 SeqScan 的 Relation
 */
int ExecInitSeqScanRelation(SeqScanState *node, SeqScanExtState *ext_state, Oid relid)
{
    Relation rel;

    if (!node || !ext_state || !OidIsValid(relid)) {
        return -1;
    }

    /* 打开 Relation */
    rel = relation_open(relid, RELMODE_READ);
    if (!rel) {
        return -1;
    }

    ext_state->ss_currentRelation = rel;

    /* 更新元组描述符 */
    /* 注：不直接访问 rel->rd_att 以避免未完整定义的类型问题 */

    return 0;
}

/**
 * @brief 获取 SeqScan 的扩展状态
 */
SeqScanExtState *ExecGetSeqScanExtState(SeqScanState *node)
{
    if (!node) return NULL;
    return (SeqScanExtState *)node->ss.ps.state;
}

/**
 * @brief 获取 SeqScan 的元组描述符
 */
ExecTupleDesc *ExecGetSeqScanTupleDesc(SeqScanState *node)
{
    if (!node) return NULL;
    return node->ss.ps.ps_TupDesc;
}