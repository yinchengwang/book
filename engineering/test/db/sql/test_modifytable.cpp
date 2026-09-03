/**
 * @file test_modifytable.cpp
 * @brief ModifyTable 执行器节点单元测试
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/sql/nodes/nodeModifyTable.h"
}

namespace {

TEST(ModifyTableTest, CreateAndDestroy) {
    ModifyTablePlan *plan = make_modifytable_plan(MODIFY_TABLE_INSERT, 123);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->operation, MODIFY_TABLE_INSERT);
    EXPECT_EQ(plan->relationOid, 123);
    free(plan);
}

TEST(ModifyTableTest, MakePlan) {
    ModifyTablePlan *p1 = make_modifytable_plan(MODIFY_TABLE_INSERT, 1);
    ModifyTablePlan *p2 = make_modifytable_plan(MODIFY_TABLE_UPDATE, 2);
    ModifyTablePlan *p3 = make_modifytable_plan(MODIFY_TABLE_DELETE, 3);
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    ASSERT_NE(p3, nullptr);
    EXPECT_EQ(p1->operation, MODIFY_TABLE_INSERT);
    EXPECT_EQ(p2->operation, MODIFY_TABLE_UPDATE);
    EXPECT_EQ(p3->operation, MODIFY_TABLE_DELETE);
    free(p1); free(p2); free(p3);
}

TEST(ModifyTableTest, NullHandling) {
    ExecEndModifyTable(nullptr);
    ExecReScanModifyTable(nullptr);
    EXPECT_EQ(ExecGetModifyTableExtState(nullptr), nullptr);
    uint64_t v = 999;
    ExecGetModifyTableStats(nullptr, &v, &v, &v);
    EXPECT_EQ(v, 0);
}

}  /* namespace */
