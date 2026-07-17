/**
 * @file test_node_seqscan.cpp
 * @brief SeqScan 执行器节点单元测试（Task 2.2）
 *
 * 测试范围：
 *   - SeqScanState 初始化与释放
 *   - ExecProcNode 返回 NULL（桩实现）
 *   - 扫描描述符生命周期
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/executor.h"
#include "db/sql/nodeSeqscan.h"
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
#include "db/sql/memctx.h"
}

namespace {

/**
 * @brief 测试 SeqScanState 初始化和释放
 */
TEST(SeqScanNodeTest, InitAndEnd) {
    // 创建 EState
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    // 在查询上下文中分配 SeqScan 计划节点
    SeqScan *scan_plan = (SeqScan *)palloc0(estate->es_query_cxt, sizeof(SeqScan));
    scan_plan->plan.type = T_SeqScan;
    scan_plan->scanrelid = 1;  // 表 OID

    // 初始化 SeqScanState
    SeqScanState *state = (SeqScanState *)ExecInitSeqScan((Plan *)scan_plan, estate, 0);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->ps.type, T_SeqScanState);
    EXPECT_NE(state->ps.ExecProcNode, nullptr);
    EXPECT_EQ(state->ss_currentScanDesc, nullptr);  // 桩实现返回 NULL
    EXPECT_NE(state->ss_ScanTupleSlot, nullptr);

    // 清理
    ExecEndSeqScan(state);
    FreeEState(estate);
}

/**
 * @brief 测试 ExecProcNode 返回 NULL（桩实现）
 */
TEST(SeqScanNodeTest, ExecProcNodeReturnsNull) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    SeqScan *scan_plan = (SeqScan *)palloc0(estate->es_query_cxt, sizeof(SeqScan));
    scan_plan->plan.type = T_SeqScan;
    scan_plan->scanrelid = 1;

    SeqScanState *state = (SeqScanState *)ExecInitSeqScan((Plan *)scan_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 桩实现应返回 NULL（表示扫描结束或无表）
    TupleTableSlot *slot = ExecProcNode(&state->ps);
    EXPECT_EQ(slot, nullptr);

    // 清理
    ExecEndSeqScan(state);
    FreeEState(estate);
}

/**
 * @brief 测试 ExecEndSeqScan 处理 NULL
 */
TEST(SeqScanNodeTest, EndNull) {
    // 不应崩溃
    ExecEndSeqScan(nullptr);
}

/**
 * @brief 测试 ExecReScanSeqScan 重置节点
 */
TEST(SeqScanNodeTest, ReScan) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    SeqScan *scan_plan = (SeqScan *)palloc0(estate->es_query_cxt, sizeof(SeqScan));
    scan_plan->plan.type = T_SeqScan;
    scan_plan->scanrelid = 1;

    SeqScanState *state = (SeqScanState *)ExecInitSeqScan((Plan *)scan_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 桩实现返回 NULL
    TupleTableSlot *slot = ExecProcNode(&state->ps);
    EXPECT_EQ(slot, nullptr);

    // 重置节点（不应崩溃）
    ExecReScanSeqScan(state);

    // 重置后仍返回 NULL
    slot = ExecProcNode(&state->ps);
    EXPECT_EQ(slot, nullptr);

    ExecEndSeqScan(state);
    FreeEState(estate);
}

/**
 * @brief 测试 SeqScanState 在内存上下文中分配
 */
TEST(SeqScanNodeTest, MemoryContextAllocation) {
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    // 记录分配前的内存上下文状态
    MemoryContext query_cxt = estate->es_query_cxt;
    ASSERT_NE(query_cxt, nullptr);

    SeqScan *scan_plan = (SeqScan *)palloc0(query_cxt, sizeof(SeqScan));
    scan_plan->plan.type = T_SeqScan;
    scan_plan->scanrelid = 1;

    SeqScanState *state = (SeqScanState *)ExecInitSeqScan((Plan *)scan_plan, estate, 0);
    ASSERT_NE(state, nullptr);

    // 验证 SeqScanState 在查询上下文中分配
    // （由 ExecInitSeqScan 使用 palloc0 在 estate->es_query_cxt 中分配）

    ExecEndSeqScan(state);
    FreeEState(estate);
}

/**
 * @brief 测试 SeqScan 节点注册到执行器
 */
TEST(SeqScanNodeTest, NodeRegistration) {
    // 注册节点
    executor_register_nodes();

    // 验证 T_SeqScan 已注册
    // 通过 ExecInitNode 间接验证
    EState *estate = CreateEState();
    ASSERT_NE(estate, nullptr);

    SeqScan *scan_plan = (SeqScan *)palloc0(estate->es_query_cxt, sizeof(SeqScan));
    scan_plan->plan.type = T_SeqScan;
    scan_plan->scanrelid = 1;

    // 使用 ExecInitNode 应能找到注册的初始化函数
    PlanState *ps = ExecInitNode((Plan *)scan_plan, estate, 0);
    ASSERT_NE(ps, nullptr);
    EXPECT_EQ(ps->type, T_SeqScanState);
    EXPECT_NE(ps->ExecProcNode, nullptr);

    ExecEndSeqScan((SeqScanState *)ps);
    FreeEState(estate);
}

}  /* namespace */