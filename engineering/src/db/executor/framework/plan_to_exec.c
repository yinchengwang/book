// engineering/src/db/executor/framework/plan_to_exec.c
#include "db/executor/executor_framework.h"
#include "db/executor/exec_node.h"
#include "db/executor/exec_states.h"
#include "db/executor/exec_operators.h"
#include "db/vectorized/vectorized.h"
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
    node->open = filter_open;
    node->next = filter_next;
    node->reset = filter_reset;
    node->close = filter_close;

    // 子节点
    if (plan->left) {
        node->left = plan_to_exec_impl(plan->left);
    }
    return node;
}

static ExecNode *convert_project_node(const plan_node_t *plan) {
    ProjectState *state = (ProjectState *)calloc(1, sizeof(ProjectState));
    if (!state) return NULL;

    ExecNode *node = (ExecNode *)calloc(1, sizeof(ExecNode));
    if (!node) {
        free(state);
        return NULL;
    }

    node->node_type = plan->type;
    node->state = state;
    node->open = project_open;
    node->next = project_next;
    node->reset = project_reset;
    node->close = project_close;

    // 子节点
    if (plan->left) {
        node->left = plan_to_exec_impl(plan->left);
    }
    return node;
}

static ExecNode *convert_hashjoin_node(const plan_node_t *plan) {
    HashJoinState *state = (HashJoinState *)calloc(1, sizeof(HashJoinState));
    if (!state) return NULL;

    // 简单取 build_key_col=0, probe_key_col=0
    // 实际应用中应从 plan->data.join.condition 解析
    state->hj = vecx_hashjoin_create(0, 0);
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

    node->node_type = plan->type;
    node->state = state;
    node->open = hashjoin_open;
    node->next = hashjoin_next;
    node->reset = hashjoin_reset;
    node->close = hashjoin_close;

    // 左右子节点
    if (plan->left) {
        node->left = plan_to_exec_impl(plan->left);
    }
    if (plan->right) {
        node->right = plan_to_exec_impl(plan->right);
    }
    return node;
}

static ExecNode *convert_hashagg_node(const plan_node_t *plan) {
    HashAggState *state = (HashAggState *)calloc(1, sizeof(HashAggState));
    if (!state) return NULL;

    // 简单取 key_col=0, measure_col=1
    // 实际应用中应从 plan->data.aggregate 解析
    state->agg = vecx_hashagg_create(0, 1);
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

    node->node_type = plan->type;
    node->state = state;
    node->open = hashagg_open;
    node->next = hashagg_next;
    node->reset = hashagg_reset;
    node->close = hashagg_close;

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
            return convert_project_node(plan);
        case PLAN_JOIN_HASH:
            return convert_hashjoin_node(plan);
        case PLAN_AGGREGATE:
            return convert_hashagg_node(plan);
        case PLAN_SORT:
            // TODO: 实现 Sort 算子
            return NULL;
        default:
            return NULL;
    }
}

ExecNode *exec_create(const plan_node_t *plan) {
    return plan_to_exec_impl(plan);
}
