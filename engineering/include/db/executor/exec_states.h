// engineering/include/db/executor/exec_states.h
#ifndef DB_EXECUTOR_EXEC_STATES_H
#define DB_EXECUTOR_EXEC_STATES_H

#include "exec_node.h"
#include "db/vectorized/vectorized.h"

/**
 * @brief SeqScan 状态
 */
typedef struct {
    int table_id;
    int ncols;
    int *col_types;
    void **col_data;
    int *col_elem_size;
    int64_t total_rows;
    int batch_size;
    vecx_source_t *source;
    VectorBlock *cur_block;
    int exhausted;
} SeqScanState;

/**
 * @brief Filter 状态
 */
typedef struct {
    vecx_pred_t pred;
    VectorBlock *cur_block;
    int exhausted;
} FilterState;

/**
 * @brief Project 状态
 */
typedef struct {
    vecx_expr_t *expr;
    VectorBlock *cur_block;
    int exhausted;
} ProjectState;

/**
 * @brief HashJoin 状态
 */
typedef struct {
    vecx_hashjoin_t *hj;
    VectorBlock *cur_block;
    int exhausted;
    int build_done;
} HashJoinState;

/**
 * @brief HashAgg 状态
 */
typedef struct {
    vecx_hashagg_t *agg;
    VectorBlock *cur_block;
    int emitted;
    int exhausted;
} HashAggState;

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_EXEC_STATES_H */
