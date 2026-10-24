#include <gtest/gtest.h>

extern "C" {
#include "db/optimizer/optimizer.h"
#include "db/executor/executor_framework.h"
}

TEST(PlanParallel, SmallPlanUntouched) {
    plan_node_t *scan = plan_node_create(PLAN_SCAN_SEQ);
    ASSERT_NE(scan, nullptr);
    scan->plan_rows = 100;                       /* 低于阈值 */

    plan_node_t *root = plan_parallelize(scan, 10000.0, 4);
    EXPECT_EQ(root, scan);                       /* 不被包裹 */
    EXPECT_EQ(root->type, PLAN_SCAN_SEQ);
    plan_node_destroy(root);
}

TEST(PlanParallel, LargeScanGetsExchangeWrapper) {
    plan_node_t *scan = plan_node_create(PLAN_SCAN_SEQ);
    ASSERT_NE(scan, nullptr);
    scan->plan_rows = 100000;                    /* 超过阈值 */

    plan_node_t *root = plan_parallelize(scan, 10000.0, 4);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->type, PLAN_EXCHANGE);
    EXPECT_EQ(root->data.exchange.mode, EXCHANGE_LOCAL);
    EXPECT_EQ(root->data.exchange.dop, 4);
    EXPECT_EQ(root->left, scan);                 /* 原子树挂在 Exchange 之下 */
    plan_node_destroy(root);
}

TEST(PlanParallel, FilterChainWrappedAtTop) {
    plan_node_t *scan = plan_node_create(PLAN_SCAN_SEQ);
    plan_node_t *filter = plan_node_create(PLAN_FILTER);
    ASSERT_NE(scan, nullptr);
    ASSERT_NE(filter, nullptr);
    filter->left = scan;
    filter->plan_rows = 50000;
    scan->plan_rows = 50000;

    plan_node_t *root = plan_parallelize(filter, 10000.0, 2);
    EXPECT_EQ(root->type, PLAN_EXCHANGE);        /* 包裹在链顶（filter 之上） */
    EXPECT_EQ(root->left, filter);
    EXPECT_EQ(root->data.exchange.dop, 2);
    plan_node_destroy(root);
}

TEST(PlanParallel, JoinNotWrapped) {
    plan_node_t *join = plan_node_create(PLAN_JOIN_HASH);
    ASSERT_NE(join, nullptr);
    join->plan_rows = 1000000;

    plan_node_t *root = plan_parallelize(join, 10000.0, 4);
    EXPECT_EQ(root, join);                       /* join 不被通用规则包裹 */
    EXPECT_EQ(root->type, PLAN_JOIN_HASH);
    plan_node_destroy(root);
}
