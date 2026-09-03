// engineering/src/db/executor/operators/hashagg_exec.c
#include "db/executor/executor_framework.h"
#include "db/executor/exec_node.h"
#include "db/executor/exec_states.h"
#include "db/executor/exec_operators.h"
#include "db/vectorized/vectorized.h"
#include <stdlib.h>

int hashagg_open(ExecNode *node) {
    HashAggState *state = (HashAggState *)node->state;
    if (!state) return -1;

    state->exhausted = 0;
    state->emitted = 0;
    state->cur_block = NULL;
    return 0;
}

VectorBlock *hashagg_next(ExecNode *node) {
    HashAggState *state = (HashAggState *)node->state;
    if (!state || state->exhausted) return NULL;

    vecx_hashagg_t *agg = state->agg;
    if (!agg) return NULL;

    // 阶段1: 读取所有输入块并累积到聚合器
    if (!state->emitted) {
        while (1) {
            VectorBlock *input = exec_next(node->left);
            if (!input) break;
            int ret = vecx_hashagg_add_block(agg, input);
            if (ret != 0) {
                state->exhausted = 1;
                return NULL;
            }
        }

        // 阶段2: 发出聚合结果
        VectorBlock *output = NULL;
        int n = vecx_hashagg_emit(agg, &output);
        if (n < 0) {
            state->exhausted = 1;
            return NULL;
        }
        state->emitted = 1;
        // n==0 with output==NULL means empty result set (exhausted)
        // n==0 with output!=NULL means valid empty result (0 groups, return empty block)
        if (!output) {
            state->exhausted = 1;
        }
        return output;
    }

    // 已发出过结果，后续调用返回 NULL
    state->exhausted = 1;
    return NULL;
}

void hashagg_reset(ExecNode *node) {
    HashAggState *state = (HashAggState *)node->state;
    if (!state) return;

    state->exhausted = 0;
    state->emitted = 0;
    if (node->left && node->left->reset) {
        node->left->reset(node->left);
    }
}

void hashagg_close(ExecNode *node) {
    HashAggState *state = (HashAggState *)node->state;
    if (!state) return;

    if (state->agg) {
        vecx_hashagg_destroy(state->agg);
        state->agg = NULL;
    }
    state->cur_block = NULL;
}

ExecNode *exec_create_hashagg(int key_col, int measure_col) {
    HashAggState *state = (HashAggState *)calloc(1, sizeof(HashAggState));
    if (!state) return NULL;

    state->agg = vecx_hashagg_create(key_col, measure_col);
    if (!state->agg) {
        free(state);
        return NULL;
    }

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) {
        vecx_hashagg_destroy(state->agg);
        free(state);
        return NULL;
    }

    node->node_type = PLAN_AGGREGATE;
    node->state = state;
    node->open = hashagg_open;
    node->next = hashagg_next;
    node->reset = hashagg_reset;
    node->close = hashagg_close;

    return node;
}
