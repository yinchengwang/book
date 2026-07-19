/**
 * @file test_planner_select.cpp
 * @brief planner.c node_to_logical SELECT 解析单元测试
 *
 * 测试范围：
 *   - 简单 SELECT * 解析
 *   - SELECT 指定列
 *   - SELECT 带 WHERE 条件
 *   - 多表连接（后续扩展）
 */

#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
extern "C" {
#include "db/sql/sql_planner.h"
#include "db/parser/sql/sql.h"
}

namespace {

/**
 * @brief 辅助：创建简单比较表达式
 *
 * 创建 binary_op 节点：left OP right
 * 如 id = 1
 */
static sql_node_t *create_compare_expr(const char *col_name, sql_node_type_t op, int int_val) {
    /* 列引用 */
    sql_node_t *col = (sql_node_t *)calloc(1, sizeof(sql_node_t));
    if (!col) return NULL;
    col->type = SQL_NODE_COLUMN_REF;
    col->u.column_ref.name = strdup(col_name);

    /* 常量 */
    sql_node_t *val = (sql_node_t *)calloc(1, sizeof(sql_node_t));
    if (!val) { free(col->u.column_ref.name); free(col); return NULL; }
    val->type = SQL_NODE_CONSTANT;
    val->u.constant.type = SQL_TYPE_INT;
    val->u.constant.int_value = int_val;

    /* 比较节点 */
    sql_node_t *cmp = (sql_node_t *)calloc(1, sizeof(sql_node_t));
    if (!cmp) { free(col->u.column_ref.name); free(col); free(val); return NULL; }
    cmp->type = SQL_NODE_BINARY_OP;
    cmp->u.binary_op.op = op;
    cmp->u.binary_op.left = col;
    cmp->u.binary_op.right = val;

    return cmp;
}

/**
 * @brief 辅助：创建列列表（含多个列名）
 */
static sql_node_t *make_column_list(const char *names[], int count) {
    sql_node_t *list = (sql_node_t *)calloc(1, sizeof(sql_node_t));
    if (!list) return nullptr;
    list->type = SQL_NODE_EXPR_LIST;  /* 解析器使用 SQL_NODE_EXPR_LIST 存储列列表 */

    list->u.list.count = count;
    list->u.list.capacity = count;
    list->u.list.items = (sql_node_t **)calloc(count, sizeof(sql_node_t *));
    if (!list->u.list.items) {
        free(list);
        return nullptr;
    }

    for (int i = 0; i < count; i++) {
        sql_node_t *col = (sql_node_t *)calloc(1, sizeof(sql_node_t));
        if (!col) continue;
        col->type = SQL_NODE_COLUMN_REF;
        col->u.column_ref.name = strdup(names[i]);
        list->u.list.items[i] = col;
    }
    return list;
}

/**
 * @brief 辅助：释放辅助构造的节点（不经过 sql_node_free，因为不是解析器分配的）
 */
static void free_manual_node(sql_node_t *node) {
    if (!node) return;
    switch (node->type) {
        case SQL_NODE_SELECT:
            if (node->u.select.table_name) free(node->u.select.table_name);
            if (node->u.select.columns) free_manual_node(node->u.select.columns);
            if (node->u.select.where_cond) free_manual_node(node->u.select.where_cond);
            break;
        case SQL_NODE_EXPR_LIST:
            for (size_t i = 0; i < node->u.list.count; i++) {
                if (node->u.list.items[i]) {
                    if (node->u.list.items[i]->u.column_ref.name)
                        free(node->u.list.items[i]->u.column_ref.name);
                    free(node->u.list.items[i]);
                }
            }
            if (node->u.list.items) free(node->u.list.items);
            break;
        case SQL_NODE_COLUMN_REF:
            if (node->u.column_ref.name) free(node->u.column_ref.name);
            break;
        case SQL_NODE_BINARY_OP:
            if (node->u.binary_op.left) free_manual_node(node->u.binary_op.left);
            if (node->u.binary_op.right) free_manual_node(node->u.binary_op.right);
            break;
        case SQL_NODE_CONSTANT:
            if (node->u.constant.str_value) free(node->u.constant.str_value);
            break;
        default:
            break;
    }
    free(node);
}

/* ============================================================
 * 测试用例
 * ============================================================ */

/**
 * @brief 测试 SELECT * FROM users 解析
 */
TEST(PlannerSelectTest, SelectStar) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    /* 构造 sql_node_t：SELECT * FROM users */
    sql_node_t *node = (sql_node_t *)calloc(1, sizeof(sql_node_t));
    ASSERT_NE(node, nullptr);
    node->type = SQL_NODE_SELECT;
    node->u.select.table_name = strdup("users");
    node->u.select.columns = NULL;  /* * 表示为 NULL */
    node->u.select.where_cond = NULL;

    /* 通过 planner_logical_plan 间接调用 node_to_logical */
    LogicalPlan *plan = planner_logical_plan(ctx, node);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->type, LOGICAL_SCAN);
    EXPECT_EQ(plan->rows, 1000.0);
    EXPECT_EQ(plan->width, 100);

    /* 清理 */
    free_manual_node(node);
    planner_free_logical_plan(plan);
    planner_destroy(ctx);
}

/**
 * @brief 测试 SELECT id, name FROM users 解析
 */
TEST(PlannerSelectTest, SelectColumns) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    /* 构造：SELECT id, name FROM users */
    const char *cols[] = {"id", "name"};
    sql_node_t *node = (sql_node_t *)calloc(1, sizeof(sql_node_t));
    ASSERT_NE(node, nullptr);
    node->type = SQL_NODE_SELECT;
    node->u.select.table_name = strdup("users");
    node->u.select.columns = make_column_list(cols, 2);
    node->u.select.where_cond = NULL;

    LogicalPlan *plan = planner_logical_plan(ctx, node);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->type, LOGICAL_SCAN);

    /* 验证 targetList 被设置 */
    EXPECT_NE(plan->targetlist, nullptr);

    /* 清理 */
    free_manual_node(node);
    planner_free_logical_plan(plan);
    planner_destroy(ctx);
}

/**
 * @brief 测试 SELECT * FROM users WHERE id = 1 解析
 */
TEST(PlannerSelectTest, SelectWithWhere) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    /* 构造：SELECT * FROM users WHERE id = 1 */
    sql_node_t *node = (sql_node_t *)calloc(1, sizeof(sql_node_t));
    ASSERT_NE(node, nullptr);
    node->type = SQL_NODE_SELECT;
    node->u.select.table_name = strdup("users");
    node->u.select.columns = NULL;
    node->u.select.where_cond = create_compare_expr("id", SQL_NODE_BINARY_OP, 1);

    LogicalPlan *plan = planner_logical_plan(ctx, node);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->type, LOGICAL_SCAN);

    /* 验证 WHERE 条件被解析为 qual */
    EXPECT_NE(plan->qual, nullptr);

    /* 清理 */
    free_manual_node(node);
    planner_free_logical_plan(plan);
    planner_destroy(ctx);
}

/**
 * @brief 测试 SELECT * FROM users WHERE id = 1 通过真实解析器
 */
TEST(PlannerSelectTest, RealParserSelect) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    /* 使用真实解析器 */
    sql_node_t *ast = sql_parse_one("SELECT id, name FROM users WHERE age > 25");
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, SQL_NODE_SELECT);
    EXPECT_NE(ast->u.select.table_name, nullptr);
    EXPECT_STREQ(ast->u.select.table_name, "users");

    /* 生成逻辑计划 */
    LogicalPlan *plan = planner_logical_plan(ctx, ast);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->type, LOGICAL_SCAN);

    /* 验证 targetList 和 qual 被设置 */
    EXPECT_NE(plan->targetlist, nullptr);
    EXPECT_NE(plan->qual, nullptr);

    /* 清理 */
    sql_node_free(ast);
    planner_free_logical_plan(plan);
    planner_destroy(ctx);
}

/**
 * @brief 测试 SELECT 与物理计划转换
 */
TEST(PlannerSelectTest, SelectToPhysical) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    /* 使用真实解析器 */
    sql_node_t *ast = sql_parse_one("SELECT * FROM users WHERE id = 42");
    ASSERT_NE(ast, nullptr);

    /* 逻辑计划 */
    LogicalPlan *logical = planner_logical_plan(ctx, ast);
    ASSERT_NE(logical, nullptr);
    EXPECT_EQ(logical->type, LOGICAL_SCAN);

    /* 物理计划转换 */
    PhysPlan *physical = planner_physical_plan(ctx, logical);
    ASSERT_NE(physical, nullptr);
    EXPECT_EQ(physical->type, PHYS_SEQ_SCAN);
    EXPECT_GE(physical->total_cost, 0.0);

    /* 清理 */
    sql_node_free(ast);
    planner_free_physical_plan(physical);
    planner_free_logical_plan(logical);
    planner_destroy(ctx);
}

/**
 * @brief 测试完整计划生成（planner_plan）
 */
TEST(PlannerSelectTest, FullPlan) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);

    sql_node_t *ast = sql_parse_one("SELECT name FROM users WHERE age = 30");
    ASSERT_NE(ast, nullptr);

    /* 完整计划生成 */
    PhysPlan *plan = planner_plan(ctx, ast);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->type, PHYS_SEQ_SCAN);

    /* 清理 */
    sql_node_free(ast);
    planner_free_physical_plan(plan);
    planner_destroy(ctx);
}

}  /* namespace */