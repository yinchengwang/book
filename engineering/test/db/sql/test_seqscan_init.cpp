/**
 * @file test_seqscan_init.cpp
 * @brief ExecSeqScan catalog 集成测试（Task 2.1）
 *
 * 验证 ExecSeqScan 在 Init 阶段正确调用 catalog：
 * 1. catalog_create_table 创建表
 * 2. ExecInitSeqScan + ExecInitSeqScanRelation 初始化扫描器
 * 3. 验证 ss_currentRelation 不为 NULL（说明 relation_open → catalog_get_table 成功）
 * 4. 验证 ExecSeqScan 真正迭代（即使空表也不崩溃）
 */

#include <gtest/gtest.h>
#include <cstring>

#include "db/catalog.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/btreeam.h"
#include "db/rel.h"

extern "C" {
#include "db/sql/sql_executor.h"
#include "db/sql/nodes/nodeSeqscan.h"
}

namespace {

/**
 * @brief SeqScan catalog 集成测试环境
 *
 * 初始化存储子系统（catalog、buffer pool、heap AM、btree AM、relation）
 */
class SeqScanInitTest : public ::testing::Test {
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
 * @brief 测试 1: ExecInitSeqScanRelation 调 catalog 获取 Relation
 *
 * 验证流程：
 * 1. 通过 catalog_create_table 创建表
 * 2. 调 ExecInitSeqScanRelation
 * 3. 验证 ss_currentRelation 不为 NULL（说明 relation_open → catalog_get_table 成功）
 */
TEST_F(SeqScanInitTest, ExecInitSeqScanRelationCallsCatalog) {
    /* 创建表 */
    uint32_t oid = create_test_table("seqscan_init_test");
    ASSERT_NE(oid, (uint32_t)0);

    /* 构造 SeqScanPlan */
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = oid;

    /* 初始化 SeqScan 执行器 */
    SeqScanState *state = ExecInitSeqScan(&plan, nullptr, 0);
    ASSERT_NE(state, nullptr) << "ExecInitSeqScan 失败";

    /* 获取扩展状态 */
    SeqScanExtState *ext_state = ExecGetSeqScanExtState(state);
    ASSERT_NE(ext_state, nullptr);

    /* ss_currentRelation 应不为 NULL（catalog 已注册表 OID）*/
    EXPECT_NE(ext_state->ss_currentRelation, nullptr)
        << "ExecInitSeqScanRelation 未通过 catalog 关联 Relation";

    /* 清理 */
    ExecEndSeqScan(state);
}

/**
 * @brief 测试 2: 不存在的 OID 不应初始化 Relation
 */
TEST_F(SeqScanInitTest, ExecInitSeqScanRelationInvalidOid) {
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = 99999;  /* 不存在的 OID */

    SeqScanState *state = ExecInitSeqScan(&plan, nullptr, 0);
    /* ExecInitSeqScan 在 scanrelid 有效时调 ExecInitSeqScanRelation，失败时应返回 NULL */
    if (state) {
        SeqScanExtState *ext_state = ExecGetSeqScanExtState(state);
        /* 即便 state 创建成功，ss_currentRelation 应为 NULL */
        EXPECT_EQ(ext_state->ss_currentRelation, nullptr);
        ExecEndSeqScan(state);
    }
    /* state 为 NULL 也算预期（InitRelation 失败时清理了 state）*/
}

/**
 * @brief 测试 3: SeqScan 完整 catalog 集成（创建表 + 扫描）
 */
TEST_F(SeqScanInitTest, FullCatalogIntegration) {
    uint32_t oid = create_test_table("seqscan_full_test");
    ASSERT_NE(oid, (uint32_t)0);

    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = oid;

    SeqScanState *state = ExecInitSeqScan(&plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    /* 执行扫描：空表返回 NULL */
    TupleTableSlot *slot = ExecSeqScan((PlanState *)state);
    EXPECT_EQ(slot, nullptr);

    ExecEndSeqScan(state);
}

/**
 * @brief 测试 4: 验证 ExecInitSeqScanRelation 单独调用
 *
 * 不通过 ExecInitSeqScan，直接测 ExecInitSeqScanRelation。
 */
TEST_F(SeqScanInitTest, DirectExecInitSeqScanRelation) {
    uint32_t oid = create_test_table("seqscan_direct_test");
    ASSERT_NE(oid, (uint32_t)0);

    /* 先创建一个 SeqScanState（不通过 ExecInitSeqScan） */
    SeqScanState state;
    memset(&state, 0, sizeof(state));
    state.ss.ps.type = EXEC_SEQ_SCAN;
    SeqScanExtState ext_state;
    memset(&ext_state, 0, sizeof(ext_state));

    /* 调 ExecInitSeqScanRelation */
    int rc = ExecInitSeqScanRelation(&state, &ext_state, oid);
    EXPECT_EQ(rc, 0) << "ExecInitSeqScanRelation 应返回 0";

    /* ss_currentRelation 应被设置 */
    EXPECT_NE(ext_state.ss_currentRelation, nullptr);

    /* 清理：ExecEndSeqScanRelation 关闭 Relation */
    /* 此处仅手动释放，不调 ExecEndSeqScan（它要求 state 通过 ExecInitSeqScan 创建）*/
}

}  /* namespace */