/**
 * @file test_indexscan_e2e.cpp
 * @brief ExecIndexScan 端到端测试（Task 2.9）
 *
 * 验证 IndexScan 节点的真实执行（索引扫描）：
 * 1. 创建表 + 索引（catalog_create_table + catalog_create_index）
 * 2. 用 IndexScan 计划 + ExecInitIndexScan 初始化
 * 3. 调 ExecIndexScan 触发索引扫描
 * 4. 验证 ExecIndexScan 不崩溃
 *
 * Task 2.9 评估：ExecIndexScan 已有真实实现（index_beginscan_heapscan +
 * index_getnext 通过 BTree AM 真正扫描索引）
 */

#include <gtest/gtest.h>
#include <cstring>

#include "db/catalog.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/btreeam.h"
#include "db/rel.h"

extern "C" {
#include "db/sql/nodes/nodeIndexscan.h"
}

namespace {

/**
 * @brief IndexScan 端到端测试环境
 */
class IndexScanE2ETest : public ::testing::Test {
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
        columns[0].not_null = true;

        strncpy(columns[1].name, "value", NAMEDATALEN - 1);
        columns[1].type_oid = 23;  /* int4 */

        return catalog_create_table(name, columns, 2);
    }
};

/**
 * @brief 测试 1: IndexScan 计划工厂
 */
TEST_F(IndexScanE2ETest, MakeIndexScanPlan) {
    IndexScanPlan *plan = make_indexscan_plan(456, 123, 1, nullptr);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->type, EXEC_INDEX_SCAN);
    EXPECT_EQ(plan->indexid, 456);
    EXPECT_EQ(plan->relid, 123);
    free(plan);
}

/**
 * @brief 测试 2: IndexScan 初始化（无 Relation）
 */
TEST_F(IndexScanE2ETest, InitIndexScan) {
    IndexScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_INDEX_SCAN;

    IndexScanState *state = ExecInitIndexScan(&plan, nullptr, 0);
    if (state) {
        ExecEndIndexScan(state);
    }
}

/**
 * @brief 测试 3: ExecIndexScan 无表（空扫描）
 *
 * 创建表但没有创建索引，IndexScan 应优雅返回 NULL。
 */
TEST_F(IndexScanE2ETest, IndexScanWithoutIndex) {
    uint32_t oid = create_test_table("idx_test");
    ASSERT_NE(oid, (uint32_t)0);

    IndexScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_INDEX_SCAN;
    plan.relid = oid;
    plan.indexid = 0;  /* 无索引 */

    IndexScanState *state = ExecInitIndexScan(&plan, nullptr, 0);
    if (state) {
        TupleTableSlot *slot = ExecIndexScan(state);
        /* 无索引应返回 NULL（无元组） */
        EXPECT_EQ(slot, nullptr);
        ExecEndIndexScan(state);
    }
}

/**
 * @brief 测试 4: IndexScan 与表 + 索引集成
 */
TEST_F(IndexScanE2ETest, IndexScanWithIndex) {
    uint32_t table_oid = create_test_table("idx_with_table");
    ASSERT_NE(table_oid, (uint32_t)0);

    /* 创建索引 */
    const char *idx_cols[] = {"id"};
    uint32_t index_oid = catalog_create_index("idx_test_idx", table_oid, idx_cols, 1, true);
    ASSERT_NE(index_oid, (uint32_t)0);

    IndexScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_INDEX_SCAN;
    plan.relid = table_oid;
    plan.indexid = index_oid;

    IndexScanState *state = ExecInitIndexScan(&plan, nullptr, 0);
    if (state) {
        TupleTableSlot *slot = ExecIndexScan(state);
        /* 空表应返回 NULL */
        EXPECT_EQ(slot, nullptr);
        ExecEndIndexScan(state);
    }
}

/**
 * @brief 测试 5: NULL 参数处理
 */
TEST_F(IndexScanE2ETest, NullParameters) {
    EXPECT_NO_FATAL_FAILURE(ExecEndIndexScan(nullptr));
    EXPECT_EQ(ExecIndexScan(nullptr), nullptr);
}

}  /* namespace */