/**
 * @file bm25_scan.h
 * @brief BM25 全文搜索算子接口
 *
 * 实现 BM25 全文搜索，实现将在 Phase 5 完成。
 */
#ifndef DB_EXECUTOR_BM25_SCAN_H
#define DB_EXECUTOR_BM25_SCAN_H

#include "db/sql/sql_executor.h"  /* PlanState, TupleTableSlot */

#ifdef __cplusplus
extern "C" {
#endif

/** BM25 搜索状态 */
typedef struct Bm25ScanState_s {
    PlanState ps;           /**< 基类 PlanState */
    void     *query;        /**< 搜索查询 */
    int       top_k;        /**< Top-K 参数 */
    void     *index;        /**< BM25 索引句柄 */
} Bm25ScanState;

/**
 * @brief 初始化 BM25 搜索
 */
Bm25ScanState *exec_bm25_scan_init(PlanState *parent,
    void *query, int top_k);

/**
 * @brief 执行 BM25 搜索，返回下一行
 */
TupleTableSlot *exec_bm25_scan_next(Bm25ScanState *state);

/**
 * @brief 关闭 BM25 搜索
 */
void exec_bm25_scan_close(Bm25ScanState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_BM25_SCAN_H */