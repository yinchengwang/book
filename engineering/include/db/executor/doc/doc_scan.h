/**
 * @file doc_scan.h
 * @brief 文档扫描算子接口
 *
 * 文档扫描算子直接调用 doc_engine，实现将在 Phase 5 完成。
 */
#ifndef DB_EXECUTOR_DOC_SCAN_H
#define DB_EXECUTOR_DOC_SCAN_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/** 文档扫描状态 */
typedef struct DocScanState_s {
    PlanState ps;           /**< 基类 PlanState */
    void     *json_path;    /**< JSONPath 表达式 */
    void     *fulltext_query; /**< 全文搜索查询 */
    int       is_fulltext;  /**< 是否为全文搜索 */
} DocScanState;

/**
 * @brief 初始化文档扫描
 */
DocScanState *exec_doc_scan_init(PlanState *parent,
    void *json_path, void *fulltext_query);

/**
 * @brief 执行文档扫描，返回下一行
 */
TupleTableSlot *exec_doc_scan_next(DocScanState *state);

/**
 * @brief 关闭文档扫描
 */
void exec_doc_scan_close(DocScanState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_DOC_SCAN_H */