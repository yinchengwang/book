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

/**
 * @brief 从 join_plan 中提取 build 和 probe 侧的键列索引
 *
 * 解析连接条件表达式，假设是形如 "build_col = probe_col" 的等值连接条件。
 * 如果无法解析（条件为空或不是二元比较），返回默认值 0。
 */
static void extract_hashjoin_keys(const join_plan_t *join, int *build_key_col, int *probe_key_col) {
    *build_key_col = 0;
    *probe_key_col = 0;

    if (!join || !join->condition) return;

    // 检查是否是二元比较操作
    if (join->condition->type == EXPR_BINARY_OP) {
        const expr_t *left = join->condition->u.binary.left;
        const expr_t *right = join->condition->u.binary.right;

        // 判断左右两侧哪个是 build 侧（通常 build 在 left，但这里简单处理）
        // 这里简化处理：直接取左右两侧的 column_id
        if (left && left->type == EXPR_COLUMN) {
            *build_key_col = left->u.column.column_id;
        }
        if (right && right->type == EXPR_COLUMN) {
            *probe_key_col = right->u.column.column_id;
        }
    }
}

static ExecNode *convert_hashjoin_node(const plan_node_t *plan) {
    HashJoinState *state = (HashJoinState *)calloc(1, sizeof(HashJoinState));
    if (!state) return NULL;

    // 从 plan->data.join 提取键列
    int build_key_col = 0;
    int probe_key_col = 0;
    extract_hashjoin_keys(&plan->data.join, &build_key_col, &probe_key_col);

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

/**
 * @brief 从 aggregate_plan 中提取 key 列和 measure 列索引
 *
 * key 列从 group_by_cols[0] 获取（如果存在），
 * measure 列从第一个聚合函数的参数表达式中提取列 ID。
 */
static void extract_hashagg_keys(const aggregate_plan_t *agg, int *key_col, int *measure_col) {
    *key_col = 0;
    *measure_col = 1;

    if (!agg) return;

    // 提取 group by key 列
    if (agg->group_by_count > 0 && agg->group_by_cols) {
        *key_col = agg->group_by_cols[0];
    }

    // 提取第一个聚合函数的参数列作为 measure
    if (agg->agg_func_count > 0 && agg->agg_funcs) {
        expr_t *arg = agg->agg_funcs[0].arg;
        if (arg && arg->type == EXPR_COLUMN) {
            *measure_col = arg->u.column.column_id;
        }
    }
}

static ExecNode *convert_hashagg_node(const plan_node_t *plan) {
    HashAggState *state = (HashAggState *)calloc(1, sizeof(HashAggState));
    if (!state) return NULL;

    // 从 plan->data.aggregate 提取键列和度量列
    int key_col = 0;
    int measure_col = 1;
    extract_hashagg_keys(&plan->data.aggregate, &key_col, &measure_col);

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
