/**
 * @file test_planner.cpp
 * @brief SQL 计划器单元测试
 */

#include <gtest/gtest.h>
extern "C" {
#include "db/sql/sql_planner.h"
#include "db/parser/sql/sql.h"
}

namespace {

/**
 * @brief 测试计划器上下文创建和销毁
 */
TEST(PlannerContextTest, CreateAndDestroy) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    planner_destroy(ctx);
}

/**
 * @brief 测试计划器配置
 */
TEST(PlannerContextTest, Config) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    /* 检查默认配置 */
    EXPECT_TRUE(ctx->config.enable_seqscan);
    EXPECT_TRUE(ctx->config.enable_indexscan);
    EXPECT_TRUE(ctx->config.enable_hashjoin);
    EXPECT_EQ(ctx->config.planner_rows, 1000.0);

    /* 修改配置 */
    PlannerConfig config;
    config.enable_seqscan = false;
    config.enable_indexscan = false;
    config.enable_hashjoin = false;
    config.planner_rows = 5000.0;

    planner_set_config(ctx, &config);
    EXPECT_FALSE(ctx->config.enable_seqscan);
    EXPECT_FALSE(ctx->config.enable_hashjoin);
    EXPECT_EQ(ctx->config.planner_rows, 5000.0);

    planner_destroy(ctx);
}

/**
 * @brief 测试逻辑计划生成（空输入）
 */
TEST(LogicalPlanTest, EmptyInput) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    /* 空输入应该创建默认扫描计划 */
    LogicalPlan *plan = planner_logical_plan(ctx, nullptr);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->type, LOGICAL_SCAN);
    EXPECT_EQ(plan->rows, 1000.0);

    free(plan);
    planner_destroy(ctx);
}

/**
 * @brief 测试物理计划生成
 */
TEST(PhysicalPlanTest, BasicConversion) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    /* 创建逻辑计划 */
    LogicalPlan *logical = planner_logical_plan(ctx, nullptr);
    ASSERT_NE(logical, nullptr);

    /* 转换为物理计划 */
    PhysPlan *physical = planner_physical_plan(ctx, logical);
    ASSERT_NE(physical, nullptr);
    EXPECT_EQ(physical->type, PHYS_SEQ_SCAN);
    EXPECT_EQ(physical->rows, 1000.0);
    EXPECT_GE(physical->total_cost, 0.0);

    free(physical);
    free(logical);
    planner_destroy(ctx);
}

/**
 * @brief 测试简单 SQL 解析和计划生成
 */
TEST(PlannerTest, SimpleSelect) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    /* 解析简单 SELECT 语句 */
    sql_node_t *ast = sql_parse_one("SELECT id, name FROM users WHERE age > 25");
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, SQL_NODE_SELECT);

    /* 生成逻辑计划 */
    LogicalPlan *logical = planner_logical_plan(ctx, ast);
    ASSERT_NE(logical, nullptr);
    EXPECT_EQ(logical->type, LOGICAL_SCAN);

    /* 生成物理计划 */
    PhysPlan *physical = planner_physical_plan(ctx, logical);
    ASSERT_NE(physical, nullptr);
    EXPECT_EQ(physical->type, PHYS_SEQ_SCAN);

    /* 清理 */
    sql_node_free(ast);
    free(physical);
    free(logical);
    planner_destroy(ctx);
}

/**
 * @brief 测试完整计划生成流程
 */
TEST(PlannerTest, FullPlanGeneration) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    /* 解析 SELECT 语句 */
    sql_node_t *ast = sql_parse_one("SELECT * FROM orders WHERE status = 'pending'");
    ASSERT_NE(ast, nullptr);

    /* 完整计划生成 */
    PhysPlan *plan = planner_plan(ctx, ast);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->type, PHYS_SEQ_SCAN);

    /* 清理 */
    sql_node_free(ast);
    free(plan);
    planner_destroy(ctx);
}

/**
 * @brief 测试代价计算
 */
TEST(CostCalculationTest, SeqScanCost) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    /* 创建扫描节点 */
    PhysScan scan_node;
    memset(&scan_node, 0, sizeof(scan_node));

    /* 计算代价：10000 行，每行 100 字节 */
    cost_seqscan(&scan_node, ctx, 10000.0, 100);

    /* 验证节点类型 */
    EXPECT_EQ(scan_node.type, PHYS_SEQ_SCAN);

    planner_destroy(ctx);
}

/**
 * @brief 测试代价计算 - 索引扫描
 */
TEST(CostCalculationTest, IndexScanCost) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    PhysScan scan_node;
    memset(&scan_node, 0, sizeof(scan_node));

    cost_index(&scan_node, ctx, 10000.0, 100);
    EXPECT_EQ(scan_node.type, PHYS_INDEX_SCAN);

    planner_destroy(ctx);
}

/**
 * @brief 测试代价计算 - Hash Join
 */
TEST(CostCalculationTest, HashJoinCost) {
    PhysJoin join_node;
    memset(&join_node, 0, sizeof(join_node));

    cost_hashjoin(&join_node, nullptr, 1000.0, 5000.0, 100);
    EXPECT_EQ(join_node.type, PHYS_HASHJOIN);
}

/**
 * @brief 测试代价计算 - 排序
 */
TEST(CostCalculationTest, SortCost) {
    PhysPlan plan;
    memset(&plan, 0, sizeof(plan));

    /* 测试小数据集 */
    cost_sort(&plan, nullptr, 100.0, 100);
    EXPECT_EQ(plan.type, PHYS_SORT);
    EXPECT_GE(plan.total_cost, 0.0);

    /* 测试大数据集 */
    cost_sort(&plan, nullptr, 100000.0, 100);
    EXPECT_GE(plan.total_cost, 0.0);
}

/**
 * @brief 测试代价计算 - 聚合
 */
TEST(CostCalculationTest, AggCost) {
    PhysAgg agg_node;
    memset(&agg_node, 0, sizeof(agg_node));

    cost_agg(&agg_node, nullptr, 10000.0, 100, 2);
    EXPECT_EQ(agg_node.type, PHYS_HASHAGG);
}

/**
 * @brief 测试优化规则应用
 */
TEST(OptimizerTest, ApplyRules) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    LogicalPlan *plan = planner_logical_plan(ctx, nullptr);
    ASSERT_NE(plan, nullptr);

    /* 应用所有优化规则 */
    planner_optimize(ctx, plan);

    /* 验证计划仍然有效 */
    EXPECT_GE(plan->node_id, 0);

    free(plan);
    planner_destroy(ctx);
}

/**
 * @brief 测试谓词下推
 */
TEST(OptimizerTest, PredicatePushdown) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    /* 解析带 WHERE 的查询 */
    sql_node_t *ast = sql_parse_one("SELECT * FROM users WHERE age > 30");
    ASSERT_NE(ast, nullptr);

    LogicalPlan *plan = planner_logical_plan(ctx, ast);
    ASSERT_NE(plan, nullptr);

    /* 应用谓词下推规则 */
    planner_apply_rule(ctx, plan, RULE_PREDICATE_PUSHDOWN);

    /* 清理 */
    sql_node_free(ast);
    free(plan);
    planner_destroy(ctx);
}

/**
 * @brief 测试常量折叠
 */
TEST(OptimizerTest, ConstantFolding) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    LogicalPlan *plan = planner_logical_plan(ctx, nullptr);
    ASSERT_NE(plan, nullptr);

    /* 应用常量折叠规则 */
    planner_apply_rule(ctx, plan, RULE_CONSTANT_FOLDING);

    free(plan);
    planner_destroy(ctx);
}

/**
 * @brief 测试逻辑计划树转换
 */
TEST(LogicalPlanTest, TreeConversion) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    /* 解析简单查询 */
    sql_node_t *ast = sql_parse_one("SELECT name FROM users WHERE id > 0");
    ASSERT_NE(ast, nullptr);

    LogicalPlan *plan = planner_logical_plan(ctx, ast);
    ASSERT_NE(plan, nullptr);

    /* 验证树结构存在 */
    EXPECT_GE(plan->node_id, 0);

    /* 清理 */
    sql_node_free(ast);
    free(plan);
    planner_destroy(ctx);
}

/**
 * @brief 测试物理计划树转换
 */
TEST(PhysicalPlanTest, TreeConversion) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    /* 解析查询 */
    sql_node_t *ast = sql_parse_one("SELECT * FROM users WHERE id > 0");
    ASSERT_NE(ast, nullptr);

    LogicalPlan *logical = planner_logical_plan(ctx, ast);
    ASSERT_NE(logical, nullptr);

    /* 转换为物理计划 */
    PhysPlan *physical = planner_physical_plan(ctx, logical);
    ASSERT_NE(physical, nullptr);

    /* 验证物理计划属性 */
    EXPECT_GE(physical->total_cost, 0.0);
    EXPECT_GE(physical->rows, 0.0);

    /* 清理 */
    sql_node_free(ast);
    free(physical);
    free(logical);
    planner_destroy(ctx);
}

/**
 * @brief 测试打印函数（不崩溃）
 */
TEST(DebugTest, DumpPlans) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    LogicalPlan *logical = planner_logical_plan(ctx, nullptr);
    if (logical) {
        EXPECT_NO_FATAL_FAILURE(planner_dump_logical(logical, 0));
        free(logical);
    }

    PhysPlan *physical = planner_physical_plan(ctx, nullptr);
    if (physical) {
        EXPECT_NO_FATAL_FAILURE(planner_dump_physical(physical, 0));
        free(physical);
    }

    /* 测试算子名称 */
    EXPECT_STREQ(planner_logical_op_name(LOGICAL_SCAN), "LogicalScan");
    EXPECT_STREQ(planner_physical_op_name(PHYS_SEQ_SCAN), "SeqScan");
    EXPECT_STREQ(planner_physical_op_name(PHYS_HASHJOIN), "HashJoin");

    planner_destroy(ctx);
}

/**
 * @brief 测试多种 SQL 语句类型
 */
TEST(PlannerTest, MultipleStatementTypes) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    /* 测试 INSERT */
    {
        sql_node_t *ast = sql_parse_one(
            "INSERT INTO users (name, age) VALUES ('Alice', 30)"
        );
        if (ast) {
            EXPECT_EQ(ast->type, SQL_NODE_INSERT);
            LogicalPlan *plan = planner_logical_plan(ctx, ast);
            if (plan) {
                EXPECT_EQ(plan->type, LOGICAL_INSERT);
                free(plan);
            }
            sql_node_free(ast);
        }
    }

    /* 测试 UPDATE */
    {
        sql_node_t *ast = sql_parse_one(
            "UPDATE users SET age = 31 WHERE id = 1"
        );
        if (ast) {
            EXPECT_EQ(ast->type, SQL_NODE_UPDATE);
            LogicalPlan *plan = planner_logical_plan(ctx, ast);
            if (plan) {
                EXPECT_EQ(plan->type, LOGICAL_UPDATE);
                free(plan);
            }
            sql_node_free(ast);
        }
    }

    /* 测试 DELETE */
    {
        sql_node_t *ast = sql_parse_one(
            "DELETE FROM users WHERE id = 1"
        );
        if (ast) {
            EXPECT_EQ(ast->type, SQL_NODE_DELETE);
            LogicalPlan *plan = planner_logical_plan(ctx, ast);
            if (plan) {
                EXPECT_EQ(plan->type, LOGICAL_DELETE);
                free(plan);
            }
            sql_node_free(ast);
        }
    }

    planner_destroy(ctx);
}

}  /* namespace */
