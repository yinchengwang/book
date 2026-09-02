// engineering/src/db/executor/operators/seqscan_exec.c
#include "db/executor/exec_node.h"
#include "db/executor/exec_states.h"
#include "db/executor/exec_operators.h"
#include "db/vectorized/vectorized.h"
#include <stdlib.h>

int seqscan_open(ExecNode *node) {
    SeqScanState *state = (SeqScanState *)node->state;
    if (!state) return -1;

    state->source = vecx_source_from_columns(
        state->ncols,
        state->col_types,
        (const void **)state->col_data,
        state->col_elem_size,
        state->total_rows,
        state->batch_size
    );

    if (!state->source) return -1;
    state->exhausted = 0;
    state->cur_block = NULL;
    return 0;
}

VectorBlock *seqscan_next(ExecNode *node) {
    SeqScanState *state = (SeqScanState *)node->state;
    if (!state || state->exhausted) return NULL;

    VectorBlock *block = vecx_source_next(state->source);
    if (!block) {
        state->exhausted = 1;
        return NULL;
    }
    return block;
}

void seqscan_reset(ExecNode *node) {
    SeqScanState *state = (SeqScanState *)node->state;
    if (!state) return;

    // Destroy and recreate the source to reset it
    if (state->source) {
        vecx_source_destroy(state->source);
        state->source = NULL;
    }

    state->source = vecx_source_from_columns(
        state->ncols,
        state->col_types,
        (const void **)state->col_data,
        state->col_elem_size,
        state->total_rows,
        state->batch_size
    );

    state->exhausted = 0;
}

void seqscan_close(ExecNode *node) {
    SeqScanState *state = (SeqScanState *)node->state;
    if (!state) return;

    if (state->source) {
        vecx_source_destroy(state->source);
        state->source = NULL;
    }
}

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
) {
    SeqScanState *state = (SeqScanState *)calloc(1, sizeof(SeqScanState));
    if (!state) return NULL;

    state->table_id = table_id;
    state->ncols = ncols;
    state->col_types = col_types;
    state->col_data = col_data;
    state->col_elem_size = col_elem_size;
    state->total_rows = total_rows;
    state->batch_size = batch_size;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) {
        free(state);
        return NULL;
    }

    node->node_type = PLAN_SCAN_SEQ;
    node->state = state;
    node->open = seqscan_open;
    node->next = seqscan_next;
    node->reset = seqscan_reset;
    node->close = seqscan_close;

    return node;
}