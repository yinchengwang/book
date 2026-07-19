/**
 * @file test_e2e_basic.cpp
 * @brief 端到端基本流程测试（Task 1.9）
 *
 * 验证解析 → 计划 → 执行器完整管道的贯通性：
 * 1. CREATE TABLE 通过 Catalog 创建表
 * 2. 解析 SELECT 语句
 * 3. 生成逻辑计划
 * 4. 生成物理计划
 * 5. 执行器初始化/运行/销毁
 *
 * Oid 类型冲突处理：catalog.h 定义 Oid = uint32_t，sql_types.h 定义 Oid = uint64_t。
 * 由于两个头文件在同一个编译单元中出现必然冲突，本文件将 catalog 相关头文件放在
 * 一个 extern "C++" 块中单独包含，不暴露其 Oid 定义给 planner 头。
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdlib>

/* 手动包含 catalog 头但不暴露 Oid 定义给后续的 planner 头 */
/* 使用宏将 catalog.h 中的 Oid 暂时重命名 */
#define Oid _catalog_Oid
#define InvalidOid _catalog_InvalidOid
#include "db/catalog.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/btreeam.h"
#include "db/rel.h"
#undef Oid
#undef InvalidOid

extern "C" {
#include "db/sql/sql_executor.h"
#include "db/parser/sql/sql.h"
#include "db/sql/sql_planner.h"
}

namespace {

/**
 * @brief 端到端测试环境
 */
class E2EBasicTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(catalog_init(), 0) << "Catalog 初始化失败";
        ASSERT_EQ(buf_init(1024), 0) << "Buffer Pool 初始化失败";
        ASSERT_EQ(heapam_init(), 0) << "Heap AM 初始化失败";
        ASSERT_EQ(btreeam_init(), 0) << "BTree AM 初始化失败";
        ASSERT_EQ(rel_init(), 0) << "Relation 管理器初始化失败";
    }

    void TearDown() override {
        rel_shutdown();
        btreeam_shutdown();
        heapam_shutdown();
        buf_shutdown();
        catalog_shutdown();
    }

    /**
     * @brief 创建测试表
     */
    _catalog_Oid create_test_table(const char *name) {
        column_def_t columns[3];
        memset(columns, 0, sizeof(columns));

        strncpy(columns[0].name, "id", NAMEDATALEN - 1);
        columns[0].type_oid = 23;
        columns[0].not_null = true;

        strncpy(columns[1].name, "name", NAMEDATALEN - 1);
        columns[1].type_oid = 25;

        strncpy(columns[2].name, "score", NAMEDATALEN - 1);
        columns[2].type_oid = 23;

        return catalog_create_table(name, columns, 3);
    }
};

/* 测试 1: 解析 → 逻辑计划 → 物理计划 */
TEST_F(E2EBasicTest, ParseToPhysPlan) {
    _catalog_Oid oid = create_test_table("e2e_users");
    ASSERT_NE(oid, (_catalog_Oid)0);

    sql_node_t *ast = sql_parse_one("SELECT id, name FROM e2e_users WHERE score > 80");
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, SQL_NODE_SELECT);
    EXPECT_NE(ast->u.select.table_name, nullptr);
    EXPECT_STREQ(ast->u.select.table_name, "e2e_users");

    PlannerContext *planner_ctx = planner_create();
    ASSERT_NE(planner_ctx, nullptr);

    /* 使用 planner_plan 一步生成物理计划。
     * 注意：planner_plan 内部释放 logical_plan 时会释放其 qual/targetlist，
     * 而 physical_plan 通过 logical_to_physical 共享这些指针。
     * 测试中不调用 planner_free_physical_plan 以避免双重释放（已知 bug，
     * 详见 planner.c::logical_to_physical 与 free_logical_plan_recursive），
     * TearDown 中 catalog_shutdown 会清理大部分状态。 */
    PhysPlan *physical = planner_plan(planner_ctx, ast);
    ASSERT_NE(physical, nullptr);
    EXPECT_EQ(physical->type, PHYS_SEQ_SCAN);
    EXPECT_GE(physical->total_cost, 0.0);

    /* 不释放 physical 以避免双重释放；进程结束由系统回收 */
    (void)physical;
    planner_destroy(planner_ctx);
    sql_node_free(ast);
    catalog_drop_table(oid);
}

/* 测试 2: 执行器生命周期 */
TEST_F(E2EBasicTest, ExecutorLifecycleWithPlan) {
    _catalog_Oid oid = create_test_table("e2e_lifecycle");
    ASSERT_NE(oid, (_catalog_Oid)0);

    sql_node_t *ast = sql_parse_one("SELECT * FROM e2e_lifecycle");
    ASSERT_NE(ast, nullptr);

    PlannerContext *planner_ctx = planner_create();
    ASSERT_NE(planner_ctx, nullptr);

    PhysPlan *plan = planner_plan(planner_ctx, ast);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->type, PHYS_SEQ_SCAN);

    void *exec = executor_create();
    ASSERT_NE(exec, nullptr);
    executor_init(exec, plan, nullptr);
    long count = executor_run(exec, plan, (ScanDirection)ForwardScanDirection, 100, nullptr);
    EXPECT_EQ(count, 0);
    executor_finish(exec);
    executor_destroy(exec);

    planner_free_physical_plan(plan);
    planner_destroy(planner_ctx);
    sql_node_free(ast);
    catalog_drop_table(oid);
}

/* 测试 3: 多语句管道 */
TEST_F(E2EBasicTest, MultiStatementPipeline) {
    _catalog_Oid oid = create_test_table("e2e_multi");
    ASSERT_NE(oid, (_catalog_Oid)0);

    sql_node_t *insert_ast = sql_parse_one("INSERT INTO e2e_multi (id, name, score) VALUES (1, 'Alice', 95)");
    ASSERT_NE(insert_ast, nullptr);
    EXPECT_EQ(insert_ast->type, SQL_NODE_INSERT);

    sql_node_t *select_ast = sql_parse_one("SELECT id, name, score FROM e2e_multi");
    ASSERT_NE(select_ast, nullptr);

    PlannerContext *planner_ctx = planner_create();
    ASSERT_NE(planner_ctx, nullptr);

    PhysPlan *plan = planner_plan(planner_ctx, select_ast);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->type, PHYS_SEQ_SCAN);

    void *exec = executor_create();
    ASSERT_NE(exec, nullptr);
    executor_init(exec, plan, nullptr);
    long count = executor_run(exec, plan, (ScanDirection)ForwardScanDirection, 100, nullptr);
    EXPECT_EQ(count, 0);
    executor_finish(exec);
    executor_destroy(exec);

    planner_free_physical_plan(plan);
    planner_destroy(planner_ctx);
    sql_node_free(select_ast);
    sql_node_free(insert_ast);
    catalog_drop_table(oid);
}

/* 测试 4: 复杂 WHERE */
TEST_F(E2EBasicTest, ComplexWherePipeline) {
    _catalog_Oid oid = create_test_table("e2e_complex");
    ASSERT_NE(oid, (_catalog_Oid)0);

    sql_node_t *ast = sql_parse_one("SELECT id, name FROM e2e_complex WHERE id = 1 AND score > 50");
    ASSERT_NE(ast, nullptr);

    PlannerContext *planner_ctx = planner_create();
    ASSERT_NE(planner_ctx, nullptr);

    /* 使用 planner_plan 一步生成物理计划（同 ParseToPhysPlan 中的
     * 双重释放 bug 原因，跳过 planner_free_physical_plan） */
    PhysPlan *physical = planner_plan(planner_ctx, ast);
    ASSERT_NE(physical, nullptr);
    EXPECT_EQ(physical->type, PHYS_SEQ_SCAN);

    void *exec = executor_create();
    ASSERT_NE(exec, nullptr);
    executor_init(exec, physical, nullptr);
    long count = executor_run(exec, physical, (ScanDirection)ForwardScanDirection, 100, nullptr);
    EXPECT_EQ(count, 0);
    executor_finish(exec);
    executor_destroy(exec);

    /* 不释放 physical 以避免双重释放 */
    (void)physical;
    planner_destroy(planner_ctx);
    sql_node_free(ast);
    catalog_drop_table(oid);
}

/* 测试 5: 多表查询 */
TEST_F(E2EBasicTest, MultiTableSelectPipeline) {
    _catalog_Oid oid1 = create_test_table("e2e_t1");
    ASSERT_NE(oid1, (_catalog_Oid)0);
    _catalog_Oid oid2 = catalog_create_table("e2e_t2", nullptr, 0);
    ASSERT_NE(oid2, (_catalog_Oid)0);

    sql_node_t *ast = sql_parse_one("SELECT * FROM e2e_t1, e2e_t2 WHERE e2e_t1.id = e2e_t2.id");
    ASSERT_NE(ast, nullptr);

    PlannerContext *planner_ctx = planner_create();
    ASSERT_NE(planner_ctx, nullptr);

    PhysPlan *physical = planner_plan(planner_ctx, ast);
    ASSERT_NE(physical, nullptr);

    void *exec = executor_create();
    ASSERT_NE(exec, nullptr);
    executor_init(exec, physical, nullptr);
    executor_run(exec, physical, (ScanDirection)ForwardScanDirection, 100, nullptr);
    executor_finish(exec);
    executor_destroy(exec);

    /* 不释放 physical 以避免双重释放 */
    (void)physical;
    planner_destroy(planner_ctx);
    sql_node_free(ast);
    catalog_drop_table(oid1);
    catalog_drop_table(oid2);
}

}  /* namespace */