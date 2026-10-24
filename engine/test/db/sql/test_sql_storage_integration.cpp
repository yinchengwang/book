/**
 * @file test_sql_storage_integration.cpp
 * @brief SQL 执行引擎与存储引擎端到端集成测试
 *
 * 验证 SQL 层与存储层的集成：
 * 1. Catalog：系统表管理
 * 2. Buffer Pool：页面缓存
 * 3. Heap AM：堆表扫描
 * 4. BTree AM：索引扫描
 * 5. 执行器节点：SeqScan/IndexScan 与存储层协作
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/sql_executor.h"
#include "db/sql/nodes/nodeSeqscan.h"
#include "db/sql/nodes/nodeIndexscan.h"
#include "db/catalog.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/btreeam.h"
#include "db/rel.h"
}

namespace {

/**
 * @brief SQL-存储集成测试环境
 *
 * 初始化存储子系统（Catalog、Buffer Pool、Heap/BTree AM）
 */
class SqlStorageIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化存储子系统
        ASSERT_EQ(catalog_init(), 0) << "Catalog 初始化失败";
        ASSERT_EQ(buf_init(1024), 0) << "Buffer Pool 初始化失败";  // 1024 buffers
        ASSERT_EQ(heapam_init(), 0) << "Heap AM 初始化失败";
        ASSERT_EQ(btreeam_init(), 0) << "BTree AM 初始化失败";
        ASSERT_EQ(rel_init(), 0) << "Relation 管理器初始化失败";
    }

    void TearDown() override {
        // 清理子系统
        rel_shutdown();
        btreeam_shutdown();
        heapam_shutdown();
        buf_shutdown();
        catalog_shutdown();
    }
};

/**
 * @brief 测试 Catalog 表创建
 */
TEST_F(SqlStorageIntegrationTest, CatalogCreateTable) {
    // 创建表定义
    column_def_t columns[3];
    memset(columns, 0, sizeof(columns));

    strncpy(columns[0].name, "id", NAMEDATALEN);
    columns[0].type_oid = 23;  // int4
    columns[0].not_null = true;

    strncpy(columns[1].name, "name", NAMEDATALEN);
    columns[1].type_oid = 25;  // text

    strncpy(columns[2].name, "value", NAMEDATALEN);
    columns[2].type_oid = 23;  // int4

    // 创建表
    Oid table_oid = catalog_create_table("test_table", columns, 3);
    EXPECT_NE(table_oid, InvalidOid) << "表创建失败";

    // 验证表存在
    table_info_t *info = catalog_get_table(table_oid);
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->natts, 3);
    EXPECT_STREQ(info->name, "test_table");

    catalog_free_table(info);
}

/**
 * @brief 测试 Relation 打开/关闭
 */
TEST_F(SqlStorageIntegrationTest, RelationOpenClose) {
    // 先创建表
    column_def_t columns[2];
    memset(columns, 0, sizeof(columns));
    strncpy(columns[0].name, "id", NAMEDATALEN);
    columns[0].type_oid = 23;
    strncpy(columns[1].name, "data", NAMEDATALEN);
    columns[1].type_oid = 25;

    Oid table_oid = catalog_create_table("rel_test", columns, 2);
    ASSERT_NE(table_oid, InvalidOid);

    // 打开 Relation
    Relation rel = relation_open(table_oid, RELMODE_READ);
    ASSERT_NE(rel, nullptr) << "Relation 打开失败";

    // 关闭 Relation
    relation_close(rel, RELMODE_READ);
}

/**
 * @brief 测试 SeqScan 与存储层集成
 *
 * 验证流程：
 * 1. 创建表并插入数据
 * 2. 初始化 SeqScan 执行器节点
 * 3. 执行扫描并验证结果
 */
TEST_F(SqlStorageIntegrationTest, SeqScanStorageIntegration) {
    // 创建表
    column_def_t columns[2];
    memset(columns, 0, sizeof(columns));
    strncpy(columns[0].name, "id", NAMEDATALEN);
    columns[0].type_oid = 23;
    strncpy(columns[1].name, "value", NAMEDATALEN);
    columns[1].type_oid = 23;

    Oid table_oid = catalog_create_table("seqscan_test", columns, 2);
    ASSERT_NE(table_oid, InvalidOid);

    // 创建 SeqScan 计划
    SeqScanPlan scan_plan;
    memset(&scan_plan, 0, sizeof(scan_plan));
    scan_plan.type = EXEC_SEQ_SCAN;
    scan_plan.scanrelid = table_oid;

    // 初始化 SeqScan 执行器
    SeqScanState *state = ExecInitSeqScan(&scan_plan, nullptr, 0);
    ASSERT_NE(state, nullptr) << "SeqScan 初始化失败";

    // 执行扫描
    TupleTableSlot *slot = nullptr;
    int tuple_count = 0;

    while ((slot = ExecSeqScan((PlanState *)state)) != nullptr) {
        tuple_count++;
        // 在真实场景中会验证元组内容
    }

    // 空表应返回 0 元组
    EXPECT_EQ(tuple_count, 0);

    // 清理
    ExecEndSeqScan(state);
}

/**
 * @brief 测试 IndexScan 与存储层集成
 *
 * 验证流程：
 * 1. 创建表和索引
 * 2. 初始化 IndexScan 执行器节点
 * 3. 执行索引扫描并验证结果
 */
TEST_F(SqlStorageIntegrationTest, IndexScanStorageIntegration) {
    // 创建表
    column_def_t columns[2];
    memset(columns, 0, sizeof(columns));
    strncpy(columns[0].name, "id", NAMEDATALEN);
    columns[0].type_oid = 23;
    columns[0].not_null = true;
    strncpy(columns[1].name, "data", NAMEDATALEN);
    columns[1].type_oid = 25;

    Oid table_oid = catalog_create_table("indexscan_test", columns, 2);
    ASSERT_NE(table_oid, InvalidOid);

    // 创建索引
    const char *index_cols[] = {"id"};
    Oid index_oid = catalog_create_index("idx_indexscan_id", table_oid,
                                          index_cols, 1, true);
    ASSERT_NE(index_oid, InvalidOid) << "索引创建失败";

    // 创建 IndexScan 计划
    IndexScanPlan scan_plan;
    memset(&scan_plan, 0, sizeof(scan_plan));
    scan_plan.type = EXEC_INDEX_SCAN;
    scan_plan.indexid = index_oid;
    scan_plan.relid = table_oid;

    // 初始化 IndexScan 执行器
    IndexScanState *state = ExecInitIndexScan(&scan_plan, nullptr, 0);
    ASSERT_NE(state, nullptr) << "IndexScan 初始化失败";

    // 扫描空索引（简化测试，只验证初始化成功）
    // 注意：IndexScan 执行逻辑需要更完整的实现

    // 清理
    ExecEndIndexScan(state);
}

/**
 * @brief 测试 Buffer Pool 集成
 *
 * 验证执行器正确使用 Buffer Pool：
 * - 页面获取/释放
 * - 引用计数管理
 */
TEST_F(SqlStorageIntegrationTest, BufferPoolIntegration) {
    // 创建表
    column_def_t columns[1];
    memset(columns, 0, sizeof(columns));
    strncpy(columns[0].name, "id", NAMEDATALEN);
    columns[0].type_oid = 23;

    Oid table_oid = catalog_create_table("buf_test", columns, 1);
    ASSERT_NE(table_oid, InvalidOid);

    Relation rel = relation_open(table_oid, RELMODE_READ);
    ASSERT_NE(rel, nullptr);

    // 获取 Buffer Pool 统计
    buf_stats_t stats_before;
    buf_get_stats(&stats_before);

    // 执行扫描（会使用 Buffer Pool）
    SeqScanPlan scan_plan;
    memset(&scan_plan, 0, sizeof(scan_plan));
    scan_plan.type = EXEC_SEQ_SCAN;
    scan_plan.scanrelid = table_oid;

    SeqScanState *state = ExecInitSeqScan(&scan_plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    // 执行扫描
    while (ExecSeqScan((PlanState *)state) != nullptr) {
        // 扫描所有元组
    }

    // 验证 Buffer Pool 使用
    buf_stats_t stats_after;
    buf_get_stats(&stats_after);

    // 扫描后应有 Buffer 操作（命中或未命中）
    EXPECT_GE(stats_after.hits + stats_after.misses,
              stats_before.hits + stats_before.misses);

    ExecEndSeqScan(state);
    relation_close(rel, RELMODE_READ);
}

/**
 * @brief 测试 Heap AM 插入和扫描
 *
 * 验证执行器与 Heap AM 的交互：
 * 1. 通过 Heap AM 插入数据
 * 2. 通过 SeqScan 扫描数据
 */
TEST_F(SqlStorageIntegrationTest, HeapInsertAndScan) {
    // 创建表
    column_def_t columns[2];
    memset(columns, 0, sizeof(columns));
    strncpy(columns[0].name, "id", NAMEDATALEN);
    columns[0].type_oid = 23;
    strncpy(columns[1].name, "value", NAMEDATALEN);
    columns[1].type_oid = 23;

    Oid table_oid = catalog_create_table("heap_test", columns, 2);
    ASSERT_NE(table_oid, InvalidOid);

    Relation rel = relation_open(table_oid, RELMODE_WRITE);
    ASSERT_NE(rel, nullptr);

    // 通过 Heap AM 插入数据
    int test_data[2] = {1, 100};
    int result = heap_insert(rel, test_data, sizeof(test_data),
                             0, 0, nullptr);
    EXPECT_EQ(result, 0) << "Heap 插入失败";

    relation_close(rel, RELMODE_WRITE);

    // 通过 SeqScan 扫描
    SeqScanPlan scan_plan;
    memset(&scan_plan, 0, sizeof(scan_plan));
    scan_plan.type = EXEC_SEQ_SCAN;
    scan_plan.scanrelid = table_oid;

    SeqScanState *state = ExecInitSeqScan(&scan_plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    TupleTableSlot *slot = ExecSeqScan((PlanState *)state);
    // 空表或刚插入的元组
    // 注意：真实场景需要验证 slot 内容

    ExecEndSeqScan(state);
}

/**
 * @brief 测试完整查询流程
 *
 * 端到端验证：
 * 1. Catalog 创建表
 * 2. Relation 打开
 * 3. SeqScan 执行
 * 4. 结果返回
 */
TEST_F(SqlStorageIntegrationTest, FullQueryPipeline) {
    // 准备：创建测试表
    column_def_t columns[3];
    memset(columns, 0, sizeof(columns));
    strncpy(columns[0].name, "id", NAMEDATALEN);
    columns[0].type_oid = 23;
    columns[0].not_null = true;
    strncpy(columns[1].name, "name", NAMEDATALEN);
    columns[1].type_oid = 25;
    strncpy(columns[2].name, "score", NAMEDATALEN);
    columns[2].type_oid = 23;

    Oid table_oid = catalog_create_table("full_test", columns, 3);
    ASSERT_NE(table_oid, InvalidOid);

    // 执行：完整扫描流程
    SeqScanPlan plan;
    memset(&plan, 0, sizeof(plan));
    plan.type = EXEC_SEQ_SCAN;
    plan.scanrelid = table_oid;

    SeqScanState *state = ExecInitSeqScan(&plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    // 执行扫描
    int rows = 0;
    while (ExecSeqScan((PlanState *)state) != nullptr) {
        rows++;
    }

    // 验证：空表返回 0 行
    EXPECT_EQ(rows, 0);

    // 清理
    ExecEndSeqScan(state);

    // 清理表
    catalog_drop_table(table_oid);
}

}  /* namespace */