// engineering/src/db/executor/operators/project_exec.c
#include "db/executor/executor_framework.h"
#include "db/executor/exec_node.h"
#include "db/executor/exec_states.h"
#include "db/executor/exec_operators.h"
#include "db/vectorized/vectorized.h"
#include <stdlib.h>

int project_open(ExecNode *node) {
    ProjectState *state = (ProjectState *)node->state;
    if (!state) return -1;

    state->exhausted = 0;
    state->cur_block = NULL;
    return 0;
}

VectorBlock *project_next(ExecNode *node) {
    ProjectState *state = (ProjectState *)node->state;
    if (!state || state->exhausted) return NULL;

    // 从子节点获取数据并投影
    VectorBlock *input = exec_next(node->left);
    if (!input) {
        state->exhausted = 1;
        return NULL;
    }

    VectorBlock *output = NULL;
    int ret = vecx_project(input, state->expr, &output);
    if (ret != 0) {
        state->exhausted = 1;
        return NULL;
    }

    return output;
}

void project_reset(ExecNode *node) {
    ProjectState *state = (ProjectState *)node->state;
    if (!state) return;

    state->exhausted = 0;
    if (node->left && node->left->reset) {
        node->left->reset(node->left);
    }
}

void project_close(ExecNode *node) {
    ProjectState *state = (ProjectState *)node->state;
    if (!state) return;

    if (state->expr) {
        vecx_expr_free(state->expr);
        state->expr = NULL;
    }
    state->cur_block = NULL;
}

ExecNode *exec_create_project(vecx_expr_t *expr) {
    ProjectState *state = (ProjectState *)calloc(1, sizeof(ProjectState));
    if (!state) return NULL;

    state->expr = expr;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) {
        free(state);
        return NULL;
    }

    node->node_type = PLAN_PROJECT;
    node->state = state;
    node->open = project_open;
    node->next = project_next;
    node->reset = project_reset;
    node->close = project_close;

    return node;
}
