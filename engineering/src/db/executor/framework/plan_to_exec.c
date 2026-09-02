// engineering/src/db/executor/framework/plan_to_exec.c
#include "db/executor/executor_framework.h"
#include "db/executor/exec_node.h"
#include "db/executor/exec_states.h"
#include "db/executor/exec_operators.h"
#include <stdlib.h>
#include <string.h>

static ExecNode *plan_to_exec_impl(const plan_node_t *plan);

/**
 * @brief 递归转换单个 plan 节点
 */
static ExecNode *convert_scan_node(const plan_node_t *plan) {
    SeqScanState *state = (SeqScanState *)calloc(1, sizeof(SeqScanState));
    if (!state) return NULL;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) {
        free(state);
        return NULL;
    }

    node->node_type = plan->type;
    node->state = state;
    node->open = seqscan_open;
    node->next = seqscan_next;
    node->reset = seqscan_reset;
    node->close = seqscan_close;
    return node;
}

static ExecNode *convert_filter_node(const plan_node_t *plan) {
    FilterState *state = (FilterState *)calloc(1, sizeof(FilterState));
    if (!state) return NULL;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) {
        free(state);
        return NULL;
    }

    node->node_type = plan->type;
    node->state = state;
    // 子节点
    if (plan->left) {
        node->left = plan_to_exec_impl(plan->left);
    }
    return node;
}

static ExecNode *plan_to_exec_impl(const plan_node_t *plan) {
    if (!plan) return NULL;

    switch (plan->type) {
        case PLAN_SCAN_SEQ:
        case PLAN_SCAN_INDEX:
        case PLAN_SCAN_VECTOR:
            return convert_scan_node(plan);
        case PLAN_FILTER:
            return convert_filter_node(plan);
        case PLAN_PROJECT:
        case PLAN_JOIN_HASH:
        case PLAN_AGGREGATE:
        case PLAN_SORT:
            // TODO: Task 3 实现
            return NULL;
        default:
            return NULL;
    }
}

ExecNode *exec_create(const plan_node_t *plan) {
    return plan_to_exec_impl(plan);
}
