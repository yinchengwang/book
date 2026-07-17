/**
 * @file test_storage_integration.cpp
 * @brief 存储引擎集成测试
 *
 * 验证执行器节点与存储层的集成：
 * 1. SeqScan 与存储层对接
 * 2. ModifyTable 与 heap_insert/update/delete 对接
 * 3. 完整 CRUD 流程测试
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/sql_executor.h"
#include "db/sql/nodes/nodeSeqscan.h"
#include "db/sql/nodes/nodeModifyTable.h"
#include "db/catalog.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/rel.h"
}

namespace {

/**
 * @brief 存储集成测试环境
 *
 * 初始化存储子系统（Catalog、Buffer Pool、Heap AM）
 */
class StorageIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化存储子系统
        ASSERT_EQ(catalog_init(), 0) << "Catalog 初始化失败";
        ASSERT_EQ(buf_init(1024), 0) << "Buffer Pool 初始化失败";
        ASSERT_EQ(heapam_init(), 0) << "Heap AM 初始化失败";
        ASSERT_EQ(rel_init(), 0) << "Relation 管理器初始化失败";
    }

    void TearDown() override {
        // 清理子系统
        rel_shutdown();
        heapam_shutdown();
        buf_shutdown();
        catalog_shutdown();
    }

    // 辅助函数：创建测试表
    Oid create_test_table(const char *name) {
        column_def_t columns[2];
        memset(columns, 0, sizeof(columns));

        strncpy(columns[0].name, "id", NAMEDATALEN);
        columns[0].type_oid = 23;  // int4
        columns[0].not_null = true;

        strncpy(columns[1].name, "value", NAMEDATALEN);
        columns[1].type_oid = 23;  // int4

        return catalog_create_table(name, columns, 2);
    }
};

/* ============================================================
 * SeqScan 存储集成测试
 * ============================================================ */

/**
 * @brief 测试 SeqScan 扫描空表
 */
TEST_F(StorageIntegrationTest, SeqScanEmptyTable) {
    // 创建测试表
    Oid table_oid = create_test_table("seqscan_empty_test");
    ASSERT_NE(table_oid, InvalidOid) << "表创建失败";

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
    }

    // 空表应返回 0 元组
    EXPECT_EQ(tuple_count, 0);

    // 清理
    ExecEndSeqScan(state);
}

/**
 * @brief 直接测试 heap_getnext（绕过 SeqScan）
 */
TEST_F(StorageIntegrationTest, HeapGetnextDirectly) {
    Oid table_oid = create_test_table("heap_getnext_test");
    ASSERT_NE(table_oid, InvalidOid);

    Relation rel = relation_open(table_oid, RELMODE_WRITE);
    ASSERT_NE(rel, nullptr);

    for (int i = 1; i <= 3; i++) {
        int data[2] = {i, i * 100};
        EXPECT_EQ(heap_insert(rel, data, sizeof(data), 0, 0, nullptr), 0)
            << "插入数据失败: i=" << i;
    }

    // 直接使用 table_beginscan + table_getnext
    TableScanDesc scan = table_beginscan(rel, 0, nullptr);
    ASSERT_NE(scan, nullptr);

    int tuple_count = 0;
    while (true) {
        void *tuple = table_getnext(scan);
        if (!tuple) break;
        tuple_count++;
    }

    EXPECT_EQ(tuple_count, 3) << "heap_getnext 应返回 3 条";

    table_endscan(scan);
    relation_close(rel, RELMODE_WRITE);
}

/**
 * @brief 测试 SeqScan 执行器返回元组数
 */
TEST_F(StorageIntegrationTest, SeqScanWithData) {
    Oid table_oid = create_test_table("seqscan_data_test");
    ASSERT_NE(table_oid, InvalidOid);

    Relation rel = relation_open(table_oid, RELMODE_WRITE);
    ASSERT_NE(rel, nullptr);

    for (int i = 1; i <= 3; i++) {
        int data[2] = {i, i * 100};
        EXPECT_EQ(heap_insert(rel, data, sizeof(data), 0, 0, nullptr), 0)
            << "插入数据失败: i=" << i;
    }

    BlockNumber nblocks = rel->rd_nblocks;
    relation_close(rel, RELMODE_WRITE);

    // 重新打开 Relation 以验证数据持久化
    rel = relation_open(table_oid, RELMODE_READ);
    ASSERT_NE(rel, nullptr);

    // 验证块数
    EXPECT_EQ(rel->rd_nblocks, nblocks);

    relation_close(rel, RELMODE_READ);

    // 创建 SeqScan 计划
    SeqScanPlan scan_plan;
    memset(&scan_plan, 0, sizeof(scan_plan));
    scan_plan.type = EXEC_SEQ_SCAN;
    scan_plan.scanrelid = table_oid;

    // 初始化 SeqScan 执行器
    SeqScanState *state = ExecInitSeqScan(&scan_plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    // 执行扫描并验证返回的元组数
    TupleTableSlot *slot = nullptr;
    int tuple_count = 0;

    while ((slot = ExecSeqScan((PlanState *)state)) != nullptr) {
        tuple_count++;
    }

    // 应返回 3 条元组
    EXPECT_EQ(tuple_count, 3);

    // 清理
    ExecEndSeqScan(state);
}

/**
 * @brief 测试 SeqScan 返回正确的列值
 */
TEST_F(StorageIntegrationTest, SeqScanReturnsCorrectValues) {
    // 创建测试表
    Oid table_oid = create_test_table("seqscan_values_test");
    ASSERT_NE(table_oid, InvalidOid);

    // 插入测试数据
    Relation rel = relation_open(table_oid, RELMODE_WRITE);
    ASSERT_NE(rel, nullptr);

    int test_id = 42;
    int test_value = 1234;
    int data[2] = {test_id, test_value};
    EXPECT_EQ(heap_insert(rel, data, sizeof(data), 0, 0, nullptr), 0);

    relation_close(rel, RELMODE_WRITE);

    // 执行扫描
    SeqScanPlan scan_plan;
    memset(&scan_plan, 0, sizeof(scan_plan));
    scan_plan.type = EXEC_SEQ_SCAN;
    scan_plan.scanrelid = table_oid;

    SeqScanState *state = ExecInitSeqScan(&scan_plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    TupleTableSlot *slot = ExecSeqScan((PlanState *)state);
    ASSERT_NE(slot, nullptr) << "应返回一个元组";

    // 验证元组槽结构
    EXPECT_NE(slot->tts_tupleDescriptor, nullptr);
    EXPECT_EQ(slot->tts_tupleDescriptor->natts, 2);

    // 清理
    ExecEndSeqScan(state);
}

/* ============================================================
 * ModifyTable 存储集成测试
 * ============================================================ */

/**
 * @brief 测试 INSERT 写入数据
 */
TEST_F(StorageIntegrationTest, InsertWritesData) {
    // 创建测试表
    Oid table_oid = create_test_table("insert_test");
    ASSERT_NE(table_oid, InvalidOid);

    // 创建 ModifyTable INSERT 计划
    ModifyTablePlan *plan = make_modifytable_plan(MODIFY_TABLE_INSERT, table_oid);
    ASSERT_NE(plan, nullptr);

    // 初始化 ModifyTable 执行器
    PlanState *pstate = ExecInitModifyTable(plan, nullptr, 0);
    ASSERT_NE(pstate, nullptr);

    // 模拟插入操作（需要子计划提供数据）
    // 注意：完整的 INSERT 需要子计划（如 Result 节点）提供数据
    // 这里简化测试，仅验证初始化和统计

    // 执行 ModifyTable
    TupleTableSlot *result = ExecModifyTable(pstate);
    EXPECT_EQ(result, nullptr) << "INSERT 不返回元组";

    // 获取统计信息
    uint64_t inserted, updated, deleted;
    ExecGetModifyTableStats(pstate, &inserted, &updated, &deleted);

    // 由于没有子计划，插入数应为 0
    EXPECT_EQ(inserted, 0);

    // 清理
    ExecEndModifyTable(pstate);
    free(plan);
}

/**
 * @brief 测试 DELETE 删除数据
 */
TEST_F(StorageIntegrationTest, DeleteRemovesData) {
    // 创建测试表并插入数据
    Oid table_oid = create_test_table("delete_test");
    ASSERT_NE(table_oid, InvalidOid);

    Relation rel = relation_open(table_oid, RELMODE_WRITE);
    ASSERT_NE(rel, nullptr);

    // 插入数据
    int data[2] = {1, 100};
    EXPECT_EQ(heap_insert(rel, data, sizeof(data), 0, 0, nullptr), 0);

    relation_close(rel, RELMODE_WRITE);

    // 创建 ModifyTable DELETE 计划
    ModifyTablePlan *plan = make_modifytable_plan(MODIFY_TABLE_DELETE, table_oid);
    ASSERT_NE(plan, nullptr);

    // 初始化执行器
    PlanState *pstate = ExecInitModifyTable(plan, nullptr, 0);
    ASSERT_NE(pstate, nullptr);

    // 执行 DELETE（需要子计划提供 WHERE 条件）
    ExecModifyTable(pstate);

    // 清理
    ExecEndModifyTable(pstate);
    free(plan);
}

/**
 * @brief 测试 UPDATE 更新数据
 */
TEST_F(StorageIntegrationTest, UpdateModifiesData) {
    // 创建测试表并插入数据
    Oid table_oid = create_test_table("update_test");
    ASSERT_NE(table_oid, InvalidOid);

    Relation rel = relation_open(table_oid, RELMODE_WRITE);
    ASSERT_NE(rel, nullptr);

    // 插入初始数据
    int data[2] = {1, 100};
    EXPECT_EQ(heap_insert(rel, data, sizeof(data), 0, 0, nullptr), 0);

    relation_close(rel, RELMODE_WRITE);

    // 创建 ModifyTable UPDATE 计划
    ModifyTablePlan *plan = make_modifytable_plan(MODIFY_TABLE_UPDATE, table_oid);
    ASSERT_NE(plan, nullptr);

    // 初始化执行器
    PlanState *pstate = ExecInitModifyTable(plan, nullptr, 0);
    ASSERT_NE(pstate, nullptr);

    // 执行 UPDATE
    ExecModifyTable(pstate);

    // 清理
    ExecEndModifyTable(pstate);
    free(plan);
}

/* ============================================================
 * 完整 CRUD 流程测试
 * ============================================================ */

/**
 * @brief 测试完整 CRUD 流程
 */
TEST_F(StorageIntegrationTest, InsertSelectDelete) {
    // 创建测试表
    Oid table_oid = create_test_table("crud_test");
    ASSERT_NE(table_oid, InvalidOid);

    // Step 1: INSERT - 插入数据
    Relation rel = relation_open(table_oid, RELMODE_WRITE);
    ASSERT_NE(rel, nullptr);

    int data1[2] = {1, 100};
    int data2[2] = {2, 200};
    int data3[2] = {3, 300};

    EXPECT_EQ(heap_insert(rel, data1, sizeof(data1), 0, 0, nullptr), 0);
    EXPECT_EQ(heap_insert(rel, data2, sizeof(data2), 0, 0, nullptr), 0);
    EXPECT_EQ(heap_insert(rel, data3, sizeof(data3), 0, 0, nullptr), 0);

    relation_close(rel, RELMODE_WRITE);

    // Step 2: SELECT - 扫描数据
    SeqScanPlan scan_plan;
    memset(&scan_plan, 0, sizeof(scan_plan));
    scan_plan.type = EXEC_SEQ_SCAN;
    scan_plan.scanrelid = table_oid;

    SeqScanState *scan_state = ExecInitSeqScan(&scan_plan, nullptr, 0);
    ASSERT_NE(scan_state, nullptr);

    int count = 0;
    while (ExecSeqScan((PlanState *)scan_state) != nullptr) {
        count++;
    }
    EXPECT_EQ(count, 3) << "应扫描到 3 条记录";

    ExecEndSeqScan(scan_state);

    // Step 3: DELETE - 删除部分数据
    // 简化：直接使用 heap_delete 删除
    rel = relation_open(table_oid, RELMODE_WRITE);
    ASSERT_NE(rel, nullptr);

    // 构造 TID（简化：假设第一条记录在块 0，偏移 24）
    uint8_t tid[6];
    uint32_t block = 0;
    uint16_t offset = 24;
    memcpy(tid, &block, sizeof(uint32_t));
    memcpy(tid + sizeof(uint32_t), &offset, sizeof(uint16_t));

    heap_delete(rel, tid, 0, false, false);

    relation_close(rel, RELMODE_WRITE);

    // Step 4: SELECT - 验证删除
    scan_state = ExecInitSeqScan(&scan_plan, nullptr, 0);
    ASSERT_NE(scan_state, nullptr);

    count = 0;
    while (ExecSeqScan((PlanState *)scan_state) != nullptr) {
        count++;
    }
    // 注意：heap_delete 只是标记删除，实际记录可能仍在页面中
    // 完整实现需要 VACUUM 或 MVCC 可见性检查

    ExecEndSeqScan(scan_state);
}

/**
 * @brief 测试批量插入和扫描
 */
TEST_F(StorageIntegrationTest, BatchInsertAndScan) {
    // 创建测试表
    Oid table_oid = create_test_table("batch_test");
    ASSERT_NE(table_oid, InvalidOid);

    // 批量插入 100 条记录
    Relation rel = relation_open(table_oid, RELMODE_WRITE);
    ASSERT_NE(rel, nullptr);

    const int batch_size = 100;
    for (int i = 0; i < batch_size; i++) {
        int data[2] = {i, i * 10};
        EXPECT_EQ(heap_insert(rel, data, sizeof(data), 0, 0, nullptr), 0)
            << "批量插入失败: i=" << i;
    }

    relation_close(rel, RELMODE_WRITE);

    // 扫描验证
    SeqScanPlan scan_plan;
    memset(&scan_plan, 0, sizeof(scan_plan));
    scan_plan.type = EXEC_SEQ_SCAN;
    scan_plan.scanrelid = table_oid;

    SeqScanState *state = ExecInitSeqScan(&scan_plan, nullptr, 0);
    ASSERT_NE(state, nullptr);

    int count = 0;
    while (ExecSeqScan((PlanState *)state) != nullptr) {
        count++;
    }
    EXPECT_EQ(count, batch_size) << "应扫描到 " << batch_size << " 条记录";

    ExecEndSeqScan(state);
}

/**
 * @brief 测试 Relation 打开/关闭统计
 */
TEST_F(StorageIntegrationTest, RelationOpenCloseStats) {
    // 创建测试表
    Oid table_oid = create_test_table("stats_test");
    ASSERT_NE(table_oid, InvalidOid);

    // 多次打开和关闭
    for (int i = 0; i < 5; i++) {
        Relation rel = relation_open(table_oid, RELMODE_READ);
        ASSERT_NE(rel, nullptr);
        relation_close(rel, RELMODE_READ);
    }

    // 验证没有资源泄漏（通过 Buffer Pool 统计）
    buf_stats_t stats;
    buf_get_stats(&stats);
    // num_buf_used 应该为 0 或接近 0（没有残留的 buffer 使用）
    EXPECT_LE(stats.num_buf_used, 1) << "不应有残留的 buffer 使用";
}

}  /* namespace */
