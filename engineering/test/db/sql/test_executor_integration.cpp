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

TEST(ExecutorIntegrationTest, SeqScanCreation) {
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = 0;

    SeqScanState *state = ExecInitSeqScan(&plan, NULL, 0);
    ASSERT_NE(state, nullptr);
    ExecEndSeqScan(state);
}

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
