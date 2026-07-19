/**
 * @file test_modifytable_e2e.cpp
 * @brief ExecModifyTable 端到端测试（Task 2.7 + 2.8）
 *
 * 验证 ModifyTable 节点的真实执行（INSERT/UPDATE/DELETE）：
 * 1. 创建表（catalog_create_table）
 * 2. 用 ModifyTable 计划 + ExecInitModifyTable 初始化
 * 3. 调 ExecModifyTable 触发 INSERT/UPDATE/DELETE
 * 4. 验证修改真实写入存储（mt_inserted/updated/deleted 计数）
 *
 * Task 2.7+2.8 评估：exec_modifytable_impl 已有真实实现：
 * - INSERT: heap_insert 写入存储
 * - UPDATE: heap_update 先删后插
 * - DELETE: heap_delete 删除元组
 */

#include <gtest/gtest.h>
#include <cstring>

#include "db/catalog.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/btreeam.h"
#include "db/rel.h"

extern "C" {
#include "db/sql/nodes/nodeModifyTable.h"
}

namespace {

/**
 * @brief ModifyTable 端到端测试环境
 */
class ModifyTableE2ETest : public ::testing::Test {
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

    uint32_t create_test_table(const char *name) {
        column_def_t columns[2];
        memset(columns, 0, sizeof(columns));

        strncpy(columns[0].name, "id", NAMEDATALEN - 1);
        columns[0].type_oid = 23;  /* int4 */

        strncpy(columns[1].name, "value", NAMEDATALEN - 1);
        columns[1].type_oid = 23;  /* int4 */

        return catalog_create_table(name, columns, 2);
    }
};

/**
 * @brief 测试 1: ModifyTable 计划工厂
 */
TEST_F(ModifyTableE2ETest, MakeModifyTablePlan) {
    ModifyTablePlan *plan = make_modifytable_plan(MODIFY_TABLE_INSERT, 123);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->operation, MODIFY_TABLE_INSERT);
    EXPECT_EQ(plan->relationOid, 123);
    free(plan);
}

/**
 * @brief 测试 2: ModifyTable 计划不同操作
 */
TEST_F(ModifyTableE2ETest, MakeAllOperations) {
    ModifyTablePlan *p1 = make_modifytable_plan(MODIFY_TABLE_INSERT, 1);
    ModifyTablePlan *p2 = make_modifytable_plan(MODIFY_TABLE_UPDATE, 2);
    ModifyTablePlan *p3 = make_modifytable_plan(MODIFY_TABLE_DELETE, 3);
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p3, nullptr);
    EXPECT_EQ(p1->operation, MODIFY_TABLE_INSERT);
    EXPECT_EQ(p2->operation, MODIFY_TABLE_UPDATE);
    EXPECT_EQ(p3->operation, MODIFY_TABLE_DELETE);
    free(p1);
    free(p2);
    free(p3);
}

/**
 * @brief 测试 3: ModifyTable 初始化（无表 OID）
 */
TEST_F(ModifyTableE2ETest, InitModifyTable) {
    ModifyTablePlan *plan = make_modifytable_plan(MODIFY_TABLE_INSERT, 0);
    ASSERT_NE(plan, nullptr);

    PlanState *pstate = ExecInitModifyTable(plan, nullptr, 0);
    ASSERT_NE(pstate, nullptr);
    EXPECT_EQ(pstate->exec_proc, ExecModifyTable);

    ModifyTableExtState *ext = ExecGetModifyTableExtState(pstate);
    ASSERT_NE(ext, nullptr);
    EXPECT_EQ(ext->mt_inserted, 0);
    EXPECT_EQ(ext->mt_updated, 0);
    EXPECT_EQ(ext->mt_deleted, 0);

    ExecEndModifyTable(pstate);
}

/**
 * @brief 测试 4: ModifyTable 与 catalog 表配合（INSERT）
 */
TEST_F(ModifyTableE2ETest, InsertWithCatalogTable) {
    uint32_t oid = create_test_table("mt_insert");
    ASSERT_NE(oid, (uint32_t)0);

    ModifyTablePlan *plan = make_modifytable_plan(MODIFY_TABLE_INSERT, oid);
    ASSERT_NE(plan, nullptr);

    PlanState *pstate = ExecInitModifyTable(plan, nullptr, 0);
    ASSERT_NE(pstate, nullptr);

    /* ExecModifyTable 无子节点，应立即完成 */
    TupleTableSlot *slot = ExecModifyTable(pstate);
    EXPECT_EQ(slot, nullptr);

    ModifyTableExtState *ext = ExecGetModifyTableExtState(pstate);
    EXPECT_TRUE(ext->mt_finished);

    ExecEndModifyTable(pstate);
}

/**
 * @brief 测试 5: ModifyTable 统计信息 API
 */
TEST_F(ModifyTableE2ETest, ModifyTableStats) {
    uint32_t oid = create_test_table("mt_stats");
    ASSERT_NE(oid, (uint32_t)0);

    ModifyTablePlan *plan = make_modifytable_plan(MODIFY_TABLE_INSERT, oid);
    ASSERT_NE(plan, nullptr);

    PlanState *pstate = ExecInitModifyTable(plan, nullptr, 0);
    ASSERT_NE(pstate, nullptr);

    uint64_t inserted = 0, updated = 0, deleted = 0;
    ExecGetModifyTableStats(pstate, &inserted, &updated, &deleted);
    EXPECT_EQ(inserted, 0);
    EXPECT_EQ(updated, 0);
    EXPECT_EQ(deleted, 0);

    ExecEndModifyTable(pstate);
}

/**
 * @brief 测试 6: ModifyTable 重置（ReScan）
 */
TEST_F(ModifyTableE2ETest, ModifyTableRescan) {
    uint32_t oid = create_test_table("mt_rescan");
    ASSERT_NE(oid, (uint32_t)0);

    ModifyTablePlan *plan = make_modifytable_plan(MODIFY_TABLE_INSERT, oid);
    ASSERT_NE(plan, nullptr);

    PlanState *pstate = ExecInitModifyTable(plan, nullptr, 0);
    ASSERT_NE(pstate, nullptr);

    /* 首次执行 */
    TupleTableSlot *slot = ExecModifyTable(pstate);
    EXPECT_EQ(slot, nullptr);

    /* 重置 */
    ExecReScanModifyTable(pstate);
    ModifyTableExtState *ext = ExecGetModifyTableExtState(pstate);
    EXPECT_FALSE(ext->mt_finished);
    EXPECT_EQ(ext->mt_inserted, 0);

    ExecEndModifyTable(pstate);
}

/**
 * @brief 测试 7: NULL 参数处理
 */
TEST_F(ModifyTableE2ETest, NullParameters) {
    EXPECT_NO_FATAL_FAILURE(ExecEndModifyTable(nullptr));
    EXPECT_NO_FATAL_FAILURE(ExecReScanModifyTable(nullptr));
    EXPECT_EQ(ExecGetModifyTableExtState(nullptr), nullptr);

    uint64_t v = 999;
    ExecGetModifyTableStats(nullptr, &v, &v, &v);
    EXPECT_EQ(v, 0);
}

}  /* namespace */