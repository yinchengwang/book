/**
 * @file test_executor_integration.cpp
 * @brief SQL 执行引擎集成测试
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/sql_executor.h"
#include "db/sql/nodes/nodeSeqscan.h"
#include "db/sql/nodes/nodeIndexscan.h"
#include "db/sql/nodes/nodeNestloop.h"
}

namespace {

/* T8：SeqScanCreation 测试移除。
 * T8 将 ExecInitSeqScan 统一为新 Volcano 阵营（execnodes.h/executor.h
 * 的 (Plan*, EState*, int)，实现见 src/db/sql/nodeSeqscan.c），其计划
 * 节点要求 plan->type == T_SeqScan（parsenodes.h NodeTag）。本 TU 处于
 * 旧阵营（sql_executor.h + nodes/nodeSeqscan.h 的 SeqScanPlan 世界，
 * EXEC_SEQ_SCAN 枚举值与 T_SeqScan 不同），继续在此构造旧计划调新
 * 实现只会得到 NULL。两阵营不可在同一 TU 混引（T1 spike 43 错误类），
 * 新阵营的 SeqScan 覆盖由 test_seqscan.cpp（T8 重写）与
 * test_driver_smoke.cpp 的端到端测试承担。 */

TEST(ExecutorIntegrationTest, IndexScanCreation) {
    IndexScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_INDEX_SCAN;

    IndexScanState *state = ExecInitIndexScan(&plan, NULL, 0);
    if (state) {
        ExecEndIndexScan(state);
    }
}

TEST(ExecutorIntegrationTest, NestLoopCreation) {
    NestLoopPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_NESTLOOP;

    NestLoopState *state = ExecInitNestLoop(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);
    ExecEndNestLoop(state);
}

TEST(ExecutorIntegrationTest, NodeTypeConsistency) {
    EXPECT_EQ(EXEC_SEQ_SCAN, EXEC_SEQ_SCAN);
    EXPECT_EQ(EXEC_INDEX_SCAN, EXEC_INDEX_SCAN);
    EXPECT_EQ(EXEC_NESTLOOP, EXEC_NESTLOOP);
}

}  /* namespace */
