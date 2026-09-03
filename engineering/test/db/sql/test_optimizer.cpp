/**
 * @file test_optimizer.cpp
 * @brief 查询优化规则单元测试
 *
 * 测试范围：
 * - 优化器配置
 * - 谓词下推
 * - 投影下推
 * - 常量折叠
 * - 连接顺序优化
 * - 子查询展开
 * - 聚合下推
 * - 排序优化
 * - 限制下推
 */
#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/optimizer.h"
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/parser/sql/parsenodes.h"
}

namespace {

/* ========================================================================
 * 优化器配置测试
 * ======================================================================== */

TEST(OptimizerTest, DefaultConfig) {
    OptimizerConfig config = get_default_optimizer_config();

    /* 验证所有规则默认启用 */
    EXPECT_TRUE(config.enable_predicate_pushdown);
    EXPECT_TRUE(config.enable_projection_pushdown);
    EXPECT_TRUE(config.enable_join_order);
    EXPECT_TRUE(config.enable_constant_folding);
    EXPECT_TRUE(config.enable_subquery_unnest);
    EXPECT_TRUE(config.enable_agg_pushdown);
    EXPECT_TRUE(config.enable_sort_elimination);
    EXPECT_TRUE(config.enable_limit_pushdown);
}

TEST(OptimizerTest, ConfigDisableRules) {
    OptimizerConfig config = get_default_optimizer_config();

    /* 禁用部分规则 */
    config.enable_predicate_pushdown = false;
    config.enable_join_order = false;

    /* 验证修改生效 */
    EXPECT_FALSE(config.enable_predicate_pushdown);
    EXPECT_TRUE(config.enable_projection_pushdown);
    EXPECT_FALSE(config.enable_join_order);
}

/* ========================================================================
 * 谓词下推测试
 * ======================================================================== */

TEST(OptimizerTest, PredicatePushdownBasic) {
    /* 创建简单计划树：Filter -> SeqScan */
    Plan *plan = (Plan *)calloc(1, sizeof(Plan));
    plan->type = T_Plan;
    plan->lefttree = NULL;
    plan->righttree = NULL;

    /* 应用谓词下推 */
    Plan *result = opt_predicate_pushdown(plan);

    /* 验证：当前实现返回原计划（框架） */
    EXPECT_EQ(result, plan);

    free(plan);
}

TEST(OptimizerTest, PredicatePushdownNull) {
    /* NULL 计划 */
    Plan *result = opt_predicate_pushdown(NULL);
    EXPECT_EQ(result, nullptr);
}

/* ========================================================================
 * 投影下推测试
 * ======================================================================== */

TEST(OptimizerTest, ProjectionPushdownBasic) {
    /* 创建简单计划树 */
    Plan *plan = (Plan *)calloc(1, sizeof(Plan));
    plan->type = T_Plan;
    plan->lefttree = NULL;
    plan->righttree = NULL;

    /* 应用投影下推 */
    Plan *result = opt_projection_pushdown(plan);

    /* 验证：当前实现返回原计划（框架） */
    EXPECT_EQ(result, plan);

    free(plan);
}

TEST(OptimizerTest, ProjectionPushdownNull) {
    Plan *result = opt_projection_pushdown(NULL);
    EXPECT_EQ(result, nullptr);
}

/* ========================================================================
 * 常量折叠测试
 * ======================================================================== */

TEST(OptimizerTest, ConstantFoldingBasic) {
    /* 创建简单计划树 */
    Plan *plan = (Plan *)calloc(1, sizeof(Plan));
    plan->type = T_Plan;
    plan->lefttree = NULL;
    plan->righttree = NULL;

    /* 应用常量折叠 */
    Plan *result = opt_constant_folding(plan);

    /* 验证：当前实现返回原计划（框架） */
    EXPECT_EQ(result, plan);

    free(plan);
}

TEST(OptimizerTest, ConstantFoldingNull) {
    Plan *result = opt_constant_folding(NULL);
    EXPECT_EQ(result, nullptr);
}

/* ========================================================================
 * 连接顺序优化测试
 * ======================================================================== */

TEST(OptimizerTest, JoinOrderBasic) {
    /* 创建简单计划树 */
    Plan *plan = (Plan *)calloc(1, sizeof(Plan));
    plan->type = T_Plan;
    plan->lefttree = NULL;
    plan->righttree = NULL;

    /* 应用连接顺序优化 */
    Plan *result = opt_join_order(plan);

    /* 验证：当前实现返回原计划（框架） */
    EXPECT_EQ(result, plan);

    free(plan);
}

TEST(OptimizerTest, JoinOrderNull) {
    Plan *result = opt_join_order(NULL);
    EXPECT_EQ(result, nullptr);
}

/* ========================================================================
 * 子查询展开测试
 * ======================================================================== */

TEST(OptimizerTest, SubqueryUnnestBasic) {
    /* 创建简单计划树 */
    Plan *plan = (Plan *)calloc(1, sizeof(Plan));
    plan->type = T_Plan;
    plan->lefttree = NULL;
    plan->righttree = NULL;

    /* 应用子查询展开 */
    Plan *result = opt_subquery_unnest(plan);

    /* 验证：当前实现返回原计划（框架） */
    EXPECT_EQ(result, plan);

    free(plan);
}

TEST(OptimizerTest, SubqueryUnnestNull) {
    Plan *result = opt_subquery_unnest(NULL);
    EXPECT_EQ(result, nullptr);
}

/* ========================================================================
 * 聚合下推测试
 * ======================================================================== */

TEST(OptimizerTest, AggPushdownBasic) {
    /* 创建简单计划树 */
    Plan *plan = (Plan *)calloc(1, sizeof(Plan));
    plan->type = T_Plan;
    plan->lefttree = NULL;
    plan->righttree = NULL;

    /* 应用聚合下推 */
    Plan *result = opt_agg_pushdown(plan);

    /* 验证：当前实现返回原计划（框架） */
    EXPECT_EQ(result, plan);

    free(plan);
}

TEST(OptimizerTest, AggPushdownNull) {
    Plan *result = opt_agg_pushdown(NULL);
    EXPECT_EQ(result, nullptr);
}

/* ========================================================================
 * 排序优化测试
 * ======================================================================== */

TEST(OptimizerTest, SortEliminationBasic) {
    /* 创建简单计划树 */
    Plan *plan = (Plan *)calloc(1, sizeof(Plan));
    plan->type = T_Plan;
    plan->lefttree = NULL;
    plan->righttree = NULL;

    /* 应用排序优化 */
    Plan *result = opt_sort_elimination(plan);

    /* 验证：当前实现返回原计划（框架） */
    EXPECT_EQ(result, plan);

    free(plan);
}

TEST(OptimizerTest, SortEliminationNull) {
    Plan *result = opt_sort_elimination(NULL);
    EXPECT_EQ(result, nullptr);
}

/* ========================================================================
 * 限制下推测试
 * ======================================================================== */

TEST(OptimizerTest, LimitPushdownBasic) {
    /* 创建简单计划树 */
    Plan *plan = (Plan *)calloc(1, sizeof(Plan));
    plan->type = T_Plan;
    plan->lefttree = NULL;
    plan->righttree = NULL;

    /* 应用限制下推 */
    Plan *result = opt_limit_pushdown(plan);

    /* 验证：当前实现返回原计划（框架） */
    EXPECT_EQ(result, plan);

    free(plan);
}

TEST(OptimizerTest, LimitPushdownNull) {
    Plan *result = opt_limit_pushdown(NULL);
    EXPECT_EQ(result, nullptr);
}

/* ========================================================================
 * 优化流程测试
 * ======================================================================== */

TEST(OptimizerTest, OptimizePlanBasic) {
    /* 创建简单计划树 */
    Plan *plan = (Plan *)calloc(1, sizeof(Plan));
    plan->type = T_Plan;
    plan->lefttree = NULL;
    plan->righttree = NULL;

    /* 使用默认配置优化 */
    Plan *result = optimize_plan(plan, NULL);

    /* 验证：优化后计划非空 */
    EXPECT_NE(result, nullptr);

    free(plan);
}

TEST(OptimizerTest, OptimizePlanWithConfig) {
    /* 创建简单计划树 */
    Plan *plan = (Plan *)calloc(1, sizeof(Plan));
    plan->type = T_Plan;
    plan->lefttree = NULL;
    plan->righttree = NULL;

    /* 自定义配置：只启用部分规则 */
    OptimizerConfig config = get_default_optimizer_config();
    config.enable_predicate_pushdown = false;
    config.enable_join_order = false;

    Plan *result = optimize_plan(plan, &config);

    /* 验证：优化后计划非空 */
    EXPECT_NE(result, nullptr);

    free(plan);
}

TEST(OptimizerTest, OptimizePlanNull) {
    Plan *result = optimize_plan(NULL, NULL);
    EXPECT_EQ(result, nullptr);
}

/* ========================================================================
 * 优化规则类型测试
 * ======================================================================== */

TEST(OptimizerTest, OptRuleEnum) {
    /* 验证枚举值连续 */
    EXPECT_EQ(OPT_RULE_PREDICATE_PUSHDOWN, 0);
    EXPECT_EQ(OPT_RULE_PROJECTION_PUSHDOWN, 1);
    EXPECT_EQ(OPT_RULE_JOIN_ORDER, 2);
    EXPECT_EQ(OPT_RULE_CONSTANT_FOLDING, 3);
    EXPECT_EQ(OPT_RULE_SUBQUERY_UNNEST, 4);
    EXPECT_EQ(OPT_RULE_AGG_PUSHDOWN, 5);
    EXPECT_EQ(OPT_RULE_SORT_ELIMINATION, 6);
    EXPECT_EQ(OPT_RULE_LIMIT_PUSHDOWN, 7);
    EXPECT_EQ(OPT_RULE_MAX, 8);
}

TEST(OptimizerTest, ApplyOptRule) {
    /* 创建简单计划树 */
    Plan *plan = (Plan *)calloc(1, sizeof(Plan));
    plan->type = T_Plan;
    plan->lefttree = NULL;
    plan->righttree = NULL;

    /* 应用单条规则 */
    Plan *result = apply_opt_rule(plan, OPT_RULE_CONSTANT_FOLDING);

    /* 验证：优化后计划非空 */
    EXPECT_NE(result, nullptr);

    free(plan);
}

}  // namespace
