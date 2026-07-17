/**
 * @file nodeSeqscan.h
 * @brief SeqScan 执行器节点头文件
 *
 * 实现顺序扫描执行器节点，基于 Volcano 迭代器模型。
 * 参考 PostgreSQL 的 nodeSeqscan.c 实现。
 *
 * 注意：SeqScanState 已在 sql_executor.h 中定义，
 * 此处扩展其功能实现。
 */
#ifndef DB_SQL_NODE_SEQSCAN_H
#define DB_SQL_NODE_SEQSCAN_H

#include "db/sql/sql_executor.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明，避免引入 rel.h 造成类型冲突 */
typedef struct RelationData *Relation;
typedef struct TableScanDescData *TableScanDesc;
typedef struct TupleDescData *TupleDesc;

/* 从 db/catalog.h 引入 Oid 类型 */
typedef uint32_t Oid;

/* ============================================================
 * SeqScan 计划节点
 * ============================================================ */

/**
 * @brief SeqScan 计划节点
 *
 * 由计划器生成，包含扫描所需的静态信息
 */
typedef struct SeqScanPlan {
    ExecutorType type;            /**< 节点类型（EXEC_SEQ_SCAN） */
    Oid scanrelid;                /**< 扫描的目标表 OID */
    List *qual;                   /**< 过滤条件列表 */
    List *targetlist;             /**< 目标列列表 */
} SeqScanPlan;

/* ============================================================
 * SeqScan 扩展状态
 * ============================================================ */

/**
 * @brief SeqScan 扩展执行状态
 *
 * 扩展 sql_executor.h 中的 SeqScanState，添加扫描状态和统计信息。
 * 通过 ScanState.ss.ps.state 指向此结构来关联。
 */
typedef struct SeqScanExtState {
    /* 扫描信息 */
    Relation ss_currentRelation;  /**< 当前扫描的 Relation */
    TableScanDesc ss_currentScanDesc; /**< 扫描描述符 */

    /* 目标列表 */
    List *ss_targetlist;          /**< 目标列列表 */
    List *ss_qual;                /**< 过滤条件列表 */

    /* 统计信息 */
    uint64_t ss_tuples_scanned;   /**< 已扫描元组数 */
    uint64_t ss_tuples_returned;  /**< 已返回元组数 */

    /* 状态标志 */
    bool ss_scan_started;         /**< 是否已开始扫描 */
    bool ss_scan_ended;           /**< 是否已结束扫描 */
} SeqScanExtState;

/* ============================================================
 * 函数声明
 * ============================================================ */

/**
 * @brief 初始化 SeqScan 执行状态
 *
 * @param node SeqScan 计划节点
 * @param estate 执行器状态（EState）
 * @param eflags 执行标志
 * @return SeqScanState 指针，失败返回 NULL
 */
SeqScanState *ExecInitSeqScan(SeqScanPlan *node, void *estate, int eflags);

/**
 * @brief 执行 SeqScan 迭代
 *
 * Volcano 模型：每次调用返回一个元组
 *
 * @param pstate PlanState 指针（实际上是 SeqScanState）
 * @return TupleTableSlot 指针，没有更多元组时返回 NULL
 */
TupleTableSlot *ExecSeqScan(PlanState *pstate);

/**
 * @brief 结束 SeqScan 执行
 *
 * 释放扫描资源
 *
 * @param node SeqScanState 指针
 */
void ExecEndSeqScan(SeqScanState *node);

/**
 * @brief 重置 SeqScan 执行状态
 *
 * 用于重新执行扫描
 *
 * @param node SeqScanState 指针
 */
void ExecReScanSeqScan(SeqScanState *node);

/**
 * @brief 初始化 SeqScan 的 Relation
 *
 * @param node SeqScanState 指针
 * @param ext_state 扩展状态
 * @param relid Relation OID
 * @return 0 成功，-1 失败
 */
int ExecInitSeqScanRelation(SeqScanState *node, SeqScanExtState *ext_state, Oid relid);

/**
 * @brief 获取 SeqScan 的扩展状态
 *
 * @param node SeqScanState 指针
 * @return SeqScanExtState 指针
 */
SeqScanExtState *ExecGetSeqScanExtState(SeqScanState *node);

/**
 * @brief 获取 SeqScan 的元组描述符
 *
 * @param node SeqScanState 指针
 * @return ExecTupleDesc 指针
 */
ExecTupleDesc *ExecGetSeqScanTupleDesc(SeqScanState *node);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODE_SEQSCAN_H */
