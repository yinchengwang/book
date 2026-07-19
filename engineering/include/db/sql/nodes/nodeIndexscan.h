/**
 * @file nodeIndexscan.h
 * @brief IndexScan 执行器节点头文件
 *
 * 实现索引扫描执行器节点，基于 Volcano 迭代器模型。
 * 参考 PostgreSQL 的 nodeIndexscan.c 实现。
 *
 * IndexScan 通过 BTree 索引进行高效查找：
 *   - 点查询：WHERE id = 1
 *   - 范围查询：WHERE id BETWEEN 1 AND 100
 *   - 排序输出：ORDER BY id
 *
 * 执行流程：
 *   1. ExecInitIndexScan：初始化索引和堆 Relation，创建扫描描述符
 *   2. ExecIndexScan：通过 index_getnext 迭代获取索引条目
 *   3. ExecEndIndexScan：关闭 Relation，释放扫描描述符
 */
#ifndef DB_SQL_NODE_INDEXSCAN_H
#define DB_SQL_NODE_INDEXSCAN_H

#include "db/sql/sql_executor.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明，避免引入 rel.h 造成类型冲突 */
typedef struct RelationData *Relation;
typedef struct IndexScanDescData *IndexScanDesc;
typedef struct TupleDescData *TupleDesc;

/* 从 db/catalog.h 引入 Oid 类型 */
typedef uint32_t Oid;

/* ============================================================
 * IndexScan 计划节点
 * ============================================================ */

/**
 * @brief IndexScan 计划节点
 *
 * 由计划器生成，包含索引扫描所需的静态信息
 */
typedef struct IndexScanPlan {
    ExecutorType type;            /**< 节点类型（EXEC_INDEX_SCAN） */
    Oid indexid;                  /**< 索引 OID */
    Oid relid;                    /**< 堆表 OID */
    int nkeys;                    /**< 扫描键数量 */
    struct ScanKeyData *key;      /**< 扫描键数组 */
    List *qual;                   /**< 过滤条件列表（非索引条件） */
    List *targetlist;             /**< 目标列列表 */
    List *indexorderby;           /**< 索引排序条件 */
} IndexScanPlan;

/* ============================================================
 * IndexScan 扩展状态
 * ============================================================ */

/**
 * @brief IndexScan 扩展执行状态
 *
 * 扩展 sql_executor.h 中的 IndexScanState，添加扫描状态和统计信息。
 * 通过 ScanState.ss.ps.state 指向此结构来关联。
 */
typedef struct IndexScanExtState {
    /* Relation 信息 */
    Relation iss_indexRelation;   /**< 索引 Relation */
    Relation iss_heapRelation;    /**< 堆 Relation */
    IndexScanDesc iss_ScanDesc;   /**< 索引扫描描述符 */

    /* 目标列表 */
    List *iss_targetlist;         /**< 目标列列表 */
    List *iss_qual;               /**< 过滤条件列表（非索引条件） */
    List *iss_indexqual;          /**< 索引条件列表 */

    /* 统计信息 */
    uint64_t iss_tuples_scanned;  /**< 已扫描索引条目数 */
    uint64_t iss_tuples_returned; /**< 已返回堆元组数 */
    uint64_t iss_index_entries;   /**< 索引条目总数 */

    /* 状态标志 */
    bool iss_scan_started;        /**< 是否已开始扫描 */
    bool iss_scan_ended;          /**< 是否已结束扫描 */
} IndexScanExtState;

/* ============================================================
 * 函数声明
 * ============================================================ */

/**
 * @brief 初始化 IndexScan 执行状态
 *
 * @param node IndexScan 计划节点
 * @param estate 执行器状态（EState）
 * @param eflags 执行标志
 * @return IndexScanState 指针，失败返回 NULL
 */
IndexScanState *ExecInitIndexScan(IndexScanPlan *node, void *estate, int eflags);

/**
 * @brief 执行 IndexScan 迭代
 *
 * Volcano 模型：每次调用返回一个元组
 *
 * @param node IndexScanState 指针
 * @return TupleTableSlot 指针，没有更多元组时返回 NULL
 */
TupleTableSlot *ExecIndexScan(IndexScanState *node);

/**
 * @brief 结束 IndexScan 执行
 *
 * 释放扫描资源
 *
 * @param node IndexScanState 指针
 */
void ExecEndIndexScan(IndexScanState *node);

/**
 * @brief 重置 IndexScan 执行状态
 *
 * 用于重新执行扫描
 *
 * @param node IndexScanState 指针
 */
void ExecReScanIndexScan(IndexScanState *node);

/**
 * @brief 初始化 IndexScan 的 Relation
 *
 * @param node IndexScanState 指针
 * @param ext_state 扩展状态
 * @param indexid 索引 OID
 * @param relid 堆表 OID
 * @return 0 成功，-1 失败
 */
int ExecInitIndexScanRelation(IndexScanState *node, IndexScanExtState *ext_state,
                              Oid indexid, Oid relid);

/**
 * @brief 获取 IndexScan 的扩展状态
 *
 * @param node IndexScanState 指针
 * @return IndexScanExtState 指针
 */
IndexScanExtState *ExecGetIndexScanExtState(IndexScanState *node);

/**
 * @brief 获取 IndexScan 的元组描述符
 *
 * @param node IndexScanState 指针
 * @return ExecTupleDesc 指针
 */
ExecTupleDesc *ExecGetIndexScanTupleDesc(IndexScanState *node);

/**
 * @brief 构造 IndexScan 计划节点
 *
 * @param indexid 索引 OID
 * @param relid 堆表 OID
 * @param nkeys 扫描键数量
 * @param key 扫描键数组
 * @return IndexScanPlan 指针
 */
IndexScanPlan *make_indexscan_plan(Oid indexid, Oid relid, int nkeys,
                                   struct ScanKeyData *key);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_NODE_INDEXSCAN_H */
