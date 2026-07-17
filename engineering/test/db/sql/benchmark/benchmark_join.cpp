/**
 * @file benchmark_join.cpp
 * @brief HashJoin 执行器节点性能基准测试（已禁用，占位实现）
 *
 * 原因：sql_executor.h（旧版类型）与 execnodes.h/nodeHashjoin.h（新版 PG 类型）之间
 * 存在 PlanState/ExprContext/TupleTableSlot/Oid/Relation 等类型冲突，
 * 同时 SeqScanPlan/HashJoinPlan 尚未实现。待 Phase 2+ PG 执行器完善后恢复。
 */

#include <gtest/gtest.h>

#include "benchmark_config.h"

/* 不包含 sql_executor.h 和 nodeHashjoin.h，避免类型冲突 */

namespace {

class BenchmarkJoinTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * @brief 占位测试：HashJoin 基准测试已禁用
 *
 * TODO: 待 PG 风格执行器（Phase 2+）完善后恢复以下测试：
 *   - HashJoin10Kx10K
 *   - HashJoin100Kx100K
 *
 * 恢复时需包含 sql_executor.h、catalog.h、buf.h、heapam.h、rel.h
 */
TEST_F(BenchmarkJoinTest, DISABLED_HashJoinPlaceholder) {
    GTEST_SKIP() << "HashJoin 基准测试已禁用，待 PG 风格执行器完善后恢复";
}

}  // namespace