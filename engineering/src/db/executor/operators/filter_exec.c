// engineering/src/db/executor/operators/filter_exec.c
#include "db/executor/executor_framework.h"
#include "db/executor/exec_node.h"
#include "db/executor/exec_states.h"
#include "db/executor/exec_operators.h"
#include "db/vectorized/vectorized.h"
#include <stdlib.h>

int filter_open(ExecNode *node) {
    FilterState *state = (FilterState *)node->state;
    if (!state) return -1;

    // 子节点已在 exec_open 时初始化
    state->exhausted = 0;
    state->cur_block = NULL;
    return 0;
}

VectorBlock *filter_next(ExecNode *node) {
    FilterState *state = (FilterState *)node->state;
    if (!state || state->exhausted) return NULL;

    /* 循环拉取：vecx_filter_block gather 出新块、不消费输入块，
     * 零匹配时 *out=NULL 仅表示"本块全被过滤"而非 EOF，
     * 必须销毁输入块后继续拉下一块，否则查询在首个全过滤块处截断 */
    for (;;) {
        VectorBlock *input = exec_next(node->left);
        if (!input) {
            state->exhausted = 1;
            return NULL;
        }

        VectorBlock *output = NULL;
        int n = vecx_filter_block(input, state->pred.col, state->pred.op,
                                  &state->pred.i64, &output);
        vector_block_destroy(input);   /* 输入块所有权在 vecx_filter_block 之后归本函数 */
        if (n < 0) return NULL;        /* 错误：视为 EOF（exhausted 留给上层/reset） */
        if (output) return output;     /* n>0：有匹配 */
        /* n==0：本块零匹配，继续拉下一块 */
    }
}

void filter_reset(ExecNode *node) {
    FilterState *state = (FilterState *)node->state;
    if (!state) return;

    state->exhausted = 0;
    if (node->left && node->left->reset) {
        node->left->reset(node->left);
    }
}

void filter_close(ExecNode *node) {
    FilterState *state = (FilterState *)node->state;
    if (!state) return;
    state->cur_block = NULL;
}

ExecNode *exec_create_filter(const vecx_pred_t *pred) {
    FilterState *state = (FilterState *)calloc(1, sizeof(FilterState));
    if (!state) return NULL;

    state->pred = *pred;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) {
        free(state);
        return NULL;
    }

    node->node_type = PLAN_FILTER;
    node->state = state;
    node->open = filter_open;
    node->next = filter_next;
    node->reset = filter_reset;
    node->close = filter_close;

    return node;
}
