// engineering/src/db/executor/operators/hashjoin_exec.c
#include "db/executor/executor_framework.h"
#include "db/executor/exec_node.h"
#include "db/executor/exec_states.h"
#include "db/executor/exec_operators.h"
#include "db/vectorized/vectorized.h"
#include <stdlib.h>

int hashjoin_open(ExecNode *node) {
    HashJoinState *state = (HashJoinState *)node->state;
    if (!state) return -1;

    state->exhausted = 0;
    state->build_done = 0;
    state->cur_block = NULL;
    return 0;
}

VectorBlock *hashjoin_next(ExecNode *node) {
    HashJoinState *state = (HashJoinState *)node->state;
    if (!state || state->exhausted) return NULL;

    vecx_hashjoin_t *hj = state->hj;
    if (!hj) return NULL;

    // 阶段1: build 侧 - 从左子节点读取所有块并构建哈希表
    if (!state->build_done) {
        while (1) {
            VectorBlock *build_block = exec_next(node->left);
            if (!build_block) break;
            int ret = vecx_hashjoin_add_build(hj, build_block);
            if (ret != 0) {
                state->exhausted = 1;
                return NULL;
            }
        }
        state->build_done = 1;
    }

    // 阶段2: probe 侧 - 从右子节点读取块并执行连接
    VectorBlock *probe_block = exec_next(node->right);
    if (!probe_block) {
        state->exhausted = 1;
        return NULL;
    }

    VectorBlock *output = NULL;
    int n = vecx_hashjoin_probe(hj, probe_block, &output);
    if (n < 0) {
        state->exhausted = 1;
        return NULL;
    }

    return output;
}

void hashjoin_reset(ExecNode *node) {
    HashJoinState *state = (HashJoinState *)node->state;
    if (!state) return;

    state->exhausted = 0;
    state->build_done = 0;
    if (node->left && node->left->reset) {
        node->left->reset(node->left);
    }
    if (node->right && node->right->reset) {
        node->right->reset(node->right);
    }
}

void hashjoin_close(ExecNode *node) {
    HashJoinState *state = (HashJoinState *)node->state;
    if (!state) return;

    if (state->hj) {
        vecx_hashjoin_destroy(state->hj);
        state->hj = NULL;
    }
    state->cur_block = NULL;
}

ExecNode *exec_create_hashjoin(int build_key_col, int probe_key_col) {
    HashJoinState *state = (HashJoinState *)calloc(1, sizeof(HashJoinState));
    if (!state) return NULL;

    state->hj = vecx_hashjoin_create(build_key_col, probe_key_col);
    if (!state->hj) {
        free(state);
        return NULL;
    }

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) {
        vecx_hashjoin_destroy(state->hj);
        free(state);
        return NULL;
    }

    node->node_type = PLAN_JOIN_HASH;
    node->state = state;
    node->open = hashjoin_open;
    node->next = hashjoin_next;
    node->reset = hashjoin_reset;
    node->close = hashjoin_close;

    return node;
}
