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

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_EXEC_OPERATORS_H */