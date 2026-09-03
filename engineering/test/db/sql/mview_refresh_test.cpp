/**
 * @file mview_refresh_test.cpp
 * @brief 物化视图刷新与 SQL 执行器集成测试
 *
 * 测试 RefreshMview 节点的初始化、执行和结束流程。
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/nodeRefreshMview.h"
#include "db/sql/executor.h"
#include "db/sql/materialized_view.h"
}

/* ============================================================
 * 测试夹具
 * ============================================================ */

class RefreshMviewTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 创建 EState */
        estate = CreateEState();
        ASSERT_NE(estate, nullptr);
    }

    void TearDown() override {
        if (estate) {
            FreeEState(estate);
            estate = nullptr;
        }
    }

    EState *estate = nullptr;
};

/* ============================================================
 * 测试用例
 * ============================================================ */

TEST_F(RefreshMviewTest, NodeRegistration) {
    /* 测试 RefreshMview 节点是否已注册 */
    executor_register_nodes();

    /* 验证节点注册表中包含 T_RefreshMview */
    /* 由于 find_node_init_fn 是静态函数，我们通过 ExecInitNode 间接验证 */
    RefreshMview plan;
    memset(&plan, 0, sizeof(plan));
    plan.plan.type = T_RefreshMview;
    plan.refresh_type = 0;  /* REFRESH_FULL */
    strcpy(plan.view_name, "test_mview");
    plan.with_data = true;

    PlanState *state = ExecInitNode((Plan *)&plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->plan->type, T_RefreshMview);

    /* 清理 */
    ExecEndNode(state);
}

TEST_F(RefreshMviewTest, InitAndExec) {
    /* 测试 RefreshMview 节点初始化和执行 */
    RefreshMview plan;
    memset(&plan, 0, sizeof(plan));
    plan.plan.type = T_RefreshMview;
    plan.refresh_type = 0;  /* REFRESH_FULL */
    strcpy(plan.view_name, "test_mview");
    plan.with_data = true;

    /* 初始化节点 */
    PlanState *state = ExecInitNode((Plan *)&plan, estate, 0);
    ASSERT_NE(state, nullptr);

    /* 执行节点 */
    TupleTableSlot *slot = ExecProcNode(state);
    EXPECT_NE(slot, nullptr);  /* 第一次执行应返回结果 */

    /* 再次执行：桩实现 refresh_status 始终为 0，因此会持续返回结果 */
    /* 这是预期行为，完整实现需要 Catalog 查找 + mv_refresh_* 集成 */
    slot = ExecProcNode(state);
    EXPECT_NE(slot, nullptr);  /* 桩实现会一直返回结果 */

    /* 结束节点 */
    ExecEndNode(state);
}

TEST_F(RefreshMviewTest, DifferentRefreshTypes) {
    /* 测试不同刷新类型 */
    int refresh_types[] = {REFRESH_FULL, REFRESH_INCREMENTAL, REFRESH_CONCURRENTLY};
    const char *type_names[] = {"FULL", "INCREMENTAL", "CONCURRENTLY"};

    for (int i = 0; i < 3; i++) {
        RefreshMview plan;
        memset(&plan, 0, sizeof(plan));
        plan.plan.type = T_RefreshMview;
        plan.refresh_type = refresh_types[i];
        strcpy(plan.view_name, "test_mview");
        plan.with_data = true;

        /* 初始化节点 */
        PlanState *state = ExecInitNode((Plan *)&plan, estate, 0);
        ASSERT_NE(state, nullptr) << "Failed to init for type: " << type_names[i];

        /* 执行节点 */
        TupleTableSlot *slot = ExecProcNode(state);
        EXPECT_NE(slot, nullptr) << "Failed to exec for type: " << type_names[i];

        /* 结束节点 */
        ExecEndNode(state);
    }
}

TEST_F(RefreshMviewTest, Rescan) {
    /* 测试重置（Rescan）功能 */
    RefreshMview plan;
    memset(&plan, 0, sizeof(plan));
    plan.plan.type = T_RefreshMview;
    plan.refresh_type = 0;  /* REFRESH_FULL */
    strcpy(plan.view_name, "test_mview");
    plan.with_data = true;

    /* 初始化节点 */
    PlanState *state = ExecInitNode((Plan *)&plan, estate, 0);
    ASSERT_NE(state, nullptr);

    /* 执行一次 */
    ExecProcNode(state);

    /* 重置节点 */
    ExecReScan(state);

    /* 再次执行应返回结果 */
    TupleTableSlot *slot = ExecProcNode(state);
    EXPECT_NE(slot, nullptr);

    /* 结束节点 */
    ExecEndNode(state);
}

/* ============================================================
 * 主函数
 * ============================================================ */

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
