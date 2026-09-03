// engineering/include/db/executor/exec_operators.h
#ifndef DB_EXECUTOR_EXEC_OPERATORS_H
#define DB_EXECUTOR_EXEC_OPERATORS_H

#include "exec_node.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SeqScan 算子函数
 */
int seqscan_open(ExecNode *node);
VectorBlock *seqscan_next(ExecNode *node);
void seqscan_reset(ExecNode *node);
void seqscan_close(ExecNode *node);

/**
 * @brief 创建 SeqScan ExecNode
 */
ExecNode *exec_create_seqscan(
    int table_id,
    int ncols,
    int *col_types,
    void **col_data,
    int *col_elem_size,
    int64_t total_rows,
    int batch_size
);

/**
 * @brief Filter 算子函数
 */
int filter_open(ExecNode *node);
VectorBlock *filter_next(ExecNode *node);
void filter_reset(ExecNode *node);
void filter_close(ExecNode *node);

/**
 * @brief 创建 Filter ExecNode
 */
ExecNode *exec_create_filter(const vecx_pred_t *pred);

/**
 * @brief Project 算子函数
 */
int project_open(ExecNode *node);
VectorBlock *project_next(ExecNode *node);
void project_reset(ExecNode *node);
void project_close(ExecNode *node);

/**
 * @brief 创建 Project ExecNode
 */
ExecNode *exec_create_project(vecx_expr_t *expr);

/**
 * @brief HashJoin 算子函数
 */
int hashjoin_open(ExecNode *node);
VectorBlock *hashjoin_next(ExecNode *node);
void hashjoin_reset(ExecNode *node);
void hashjoin_close(ExecNode *node);

/**
 * @brief 创建 HashJoin ExecNode
 */
ExecNode *exec_create_hashjoin(int build_key_col, int probe_key_col);

/**
 * @brief HashAgg 算子函数
 */
int hashagg_open(ExecNode *node);
VectorBlock *hashagg_next(ExecNode *node);
void hashagg_reset(ExecNode *node);
void hashagg_close(ExecNode *node);

/**
 * @brief 创建 HashAgg ExecNode
 */
ExecNode *exec_create_hashagg(int key_col, int measure_col);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_EXEC_OPERATORS_H */
