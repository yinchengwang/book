/* optimizer_parallel.c - Gap#4 并行计划重写 */
#include "db/optimizer/optimizer.h"
#include <stdlib.h>
#include <string.h>

static int is_parallelizable_chain_node(plan_node_type_t t) {
    return t == PLAN_SCAN_SEQ || t == PLAN_FILTER || t == PLAN_PROJECT;
}

/* 判断以 node 为根的子树是否整条都是 scan/filter/project 链 */
static int is_parallelizable_chain(const plan_node_t *node) {
    const plan_node_t *cur = node;
    while (cur) {
        if (!is_parallelizable_chain_node(cur->type)) return 0;
        if (cur->right || cur->subplan) return 0;   /* 只处理单链 */
        cur = cur->left;
    }
    return 1;
}

static plan_node_t *wrap_exchange(plan_node_t *subtree, int dop) {
    plan_node_t *ex = plan_node_create(PLAN_EXCHANGE);
    if (!ex) return subtree;
    ex->data.exchange.mode = EXCHANGE_LOCAL;
    ex->data.exchange.dop = dop;
    ex->data.exchange.key_col = -1;
    ex->plan_rows = subtree->plan_rows;
    ex->total_cost = subtree->total_cost;
    ex->left = subtree;
    return ex;
}

plan_node_t *plan_parallelize(plan_node_t *plan, double min_rows, int max_dop) {
    if (!plan || max_dop <= 1) return plan;
    if (plan->type == PLAN_EXCHANGE) return plan;   /* 不重复包裹 */

    /* 本节点是完整 scan/filter/project 单链的链顶且超过阈值：
     * 整条链一起包裹，不再下钻（否则内层先被包裹会截断链顶判定） */
    if (is_parallelizable_chain_node(plan->type)
        && plan->plan_rows >= min_rows
        && is_parallelizable_chain(plan)) {
        return wrap_exchange(plan, max_dop);
    }

    /* 否则递归处理子树（join 的左右侧各自可能是可并行链） */
    if (plan->left) plan->left = plan_parallelize(plan->left, min_rows, max_dop);
    if (plan->right) plan->right = plan_parallelize(plan->right, min_rows, max_dop);
    return plan;
}
