/**
 * @file storage_integration_test.cpp
 * @brief 存储架构集成测试
 *
 * 测试 Catalog、Buffer Pool、Relation、WAL 的集成
 */

#include "gtest/gtest.h"
#include "db/catalog.h"
#include "db/buf.h"
#include "db/rel.h"
#include "db/heapam.h"
#include "db/wal.h"

class StorageIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(0, catalog_init());
        ASSERT_EQ(0, buf_init(64));
        ASSERT_EQ(0, rel_init());
        ASSERT_EQ(0, heapam_init());
    }

    void TearDown() override {
        heapam_shutdown();
        rel_shutdown();
        buf_shutdown();
        catalog_shutdown();
    }
};

/* ============================================================
 * Catalog + Relation 集成测试
 * ============================================================ */

TEST_F(StorageIntegrationTest, CatalogAndRelation) {
    /* 创建表 */
    column_def_t cols[3];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;
    cols[0].not_null = true;

    strcpy(cols[1].name, "name");
    cols[1].type_oid = 25;
    cols[1].not_null = false;

    strcpy(cols[2].name, "age");
    cols[2].type_oid = 23;
    cols[2].not_null = false;

    Oid table_oid = catalog_create_table("users", cols, 3);
    ASSERT_NE(InvalidOid, table_oid);

    /* 通过 Relation 打开表 */
    Relation rel = relation_open(table_oid, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    /* 验证 TupleDesc */
    TupleDesc tdesc = relation_getdesc(rel);
    ASSERT_NE(nullptr, tdesc);
    EXPECT_EQ(3, tdesc->natts);

    relation_close(rel, 0);
}

TEST_F(StorageIntegrationTest, IndexCreation) {
    /* 创建表 */
    column_def_t cols[2];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;
    strcpy(cols[1].name, "value");
    cols[1].type_oid = 25;

    Oid table_oid = catalog_create_table("indexed_table", cols, 2);
    ASSERT_NE(InvalidOid, table_oid);

    /* 创建索引 */
    const char *idx_cols[] = {"id"};
    Oid index_oid = catalog_create_index("idx_id", table_oid, idx_cols, 1, true);
    ASSERT_NE(InvalidOid, index_oid);

    /* 获取索引列表 */
    int nindexes = 0;
    index_info_t *indexes = catalog_get_indexes(table_oid, &nindexes);
    ASSERT_NE(nullptr, indexes);
    EXPECT_EQ(1, nindexes);

    /* 清理 */
    for (int i = 0; i < nindexes; i++) {
        free(indexes[i].key_nums);
    }
    free(indexes);
}

/* ============================================================
 * Buffer Pool 测试
 * ============================================================ */

TEST_F(StorageIntegrationTest, BufferPoolBasics) {
    /* 读取页面 */
    BufferDesc *buf = buf_read(1, 0, 0);
    ASSERT_NE(nullptr, buf);
    EXPECT_TRUE(buf_isvalid(buf));
    buf_unpin(buf);

    /* 再次读取同一页面（应该命中） */
    BufferDesc *buf2 = buf_read(1, 0, 0);
    ASSERT_NE(nullptr, buf2);
    buf_unpin(buf2);

    /* 读取可写页面 */
    BufferDesc *buf3 = buf_read(2, 0, 1);
    ASSERT_NE(nullptr, buf3);
    EXPECT_TRUE(buf_isdirty(buf3));
    buf_unpin(buf3);
}

TEST_F(StorageIntegrationTest, BufferPoolReplacement) {
    /* 读取大量页面触发置换 */
    const int n = 100;
    BufferDesc *bufs[100];

    for (int i = 0; i < n; i++) {
        bufs[i] = buf_read(1, (BlockNumber)i, 0);
        ASSERT_NE(nullptr, bufs[i]);
    }

    /* 释放所有 buffer */
    for (int i = 0; i < n; i++) {
        buf_unpin(bufs[i]);
    }

    /* 验证统计 */
    buf_stats_t stats;
    buf_get_stats(&stats);
    EXPECT_GE(stats.misses, (uint64_t)n);
}

/* ============================================================
 * Heap AM 测试
 * ============================================================ */

TEST_F(StorageIntegrationTest, HeapPageOperations) {
    char page[HEAP_PAGE_SIZE];
    heap_page_init(page, HEAP_PAGE_SIZE);

    /* 验证页面初始化 */
    PageHeader ph = heap_page_get_header(page);
    EXPECT_EQ(SizeOfPageHeaderData, ph->pd_lower);
    EXPECT_EQ(HEAP_PAGE_SIZE, ph->pd_upper);

    /* 添加元组 */
    const char *tuples[] = {"apple", "banana", "cherry"};
    for (int i = 0; i < 3; i++) {
        HeapLinePointer lp = NULL;
        int r = heap_page_add_tuple(page, tuples[i], strlen(tuples[i]) + 1, &lp);
        EXPECT_EQ(0, r);
        ASSERT_NE(nullptr, lp);
    }

    /* 验证元组数量 */
    EXPECT_EQ(3, heap_page_get_tuple_count(page));
}

TEST_F(StorageIntegrationTest, HeapInsertAndScan) {
    Relation rel = relation_opennode(100, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    /* 插入元组 */
    const char *tuples[] = {"row1", "row2", "row3"};
    for (int i = 0; i < 3; i++) {
        int r = heap_insert(rel, tuples[i], strlen(tuples[i]), 0, 0, NULL);
        EXPECT_EQ(0, r);
    }

    /* 更新块计数（简化） */
    rel->rd_nblocks = 1;

    /* 开始扫描 */
    TableScanDesc scan = table_beginscan_all(rel);
    ASSERT_NE(nullptr, scan);

    /* 获取元组 */
    int count = 0;
    while (heap_getnext(scan, ForwardScanDirection) != NULL) {
        count++;
    }
    EXPECT_EQ(3, count);

    table_endscan(scan);
    relation_close(rel, 0);
}

/* ============================================================
 * 综合集成测试
 * ============================================================ */

TEST_F(StorageIntegrationTest, FullWorkflow) {
    /* 1. 通过 Catalog 创建表 */
    column_def_t cols[2];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;
    cols[0].not_null = true;
    strcpy(cols[1].name, "data");
    cols[1].type_oid = 25;
    cols[1].not_null = false;

    Oid table_oid = catalog_create_table("workflow_test", cols, 2);
    ASSERT_NE(InvalidOid, table_oid);

    /* 2. 通过 Relation 打开表 */
    Relation rel = relation_open(table_oid, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    /* 3. 通过 Buffer Pool 读取/写入页面 */
    BufferDesc *buf = buf_new(rel->rd_relfilenode);
    ASSERT_NE(nullptr, buf);
    buf_dirty(buf);
    buf_unpin(buf);

    /* 4. 通过 Heap AM 插入元组 */
    const char *row = "test_row_data";
    EXPECT_EQ(0, heap_insert(rel, row, strlen(row), 0, 0, NULL));

    /* 5. 关闭 Relation */
    relation_close(rel, 0);

    /* 6. 验证 Catalog 中的表信息 */
    table_info_t *info = catalog_get_table(table_oid);
    ASSERT_NE(nullptr, info);
    EXPECT_STREQ("workflow_test", info->name);
    EXPECT_EQ(2, info->natts);
}

TEST_F(StorageIntegrationTest, CacheHitRate) {
    /* 重置统计 */
    buf_reset_stats();

    /* 创建表 */
    column_def_t cols[1];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;

    Oid table_oid = catalog_create_table("cache_test", cols, 1);
    ASSERT_NE(InvalidOid, table_oid);

    /* 重复访问同一页面 */
    BufferDesc *buf;
    for (int i = 0; i < 10; i++) {
        buf = buf_read(table_oid, 0, 0);
        ASSERT_NE(nullptr, buf);
        buf_unpin(buf);
    }

    /* 验证缓存命中 */
    buf_stats_t stats;
    buf_get_stats(&stats);
    EXPECT_GE(stats.hits, 9u);  /* 9 次命中 */
    EXPECT_EQ(1u, stats.misses);  /* 1 次未命中 */
}

/* ============================================================
 * 错误处理测试
 * ============================================================ */

TEST_F(StorageIntegrationTest, ErrorHandling) {
    /* 查找不存在的表 */
    Oid oid = catalog_lookup_table("nonexistent_table");
    EXPECT_EQ(InvalidOid, oid);

    /* 打开不存在的 Relation */
    Relation rel = relation_open(InvalidOid, REL_OPEN_READONLY);
    EXPECT_EQ(nullptr, rel);

    /* 读取无效页面 */
    BufferDesc *buf = buf_read(0, 0, 0);
    /* buf 可能非 NULL（Buffer 0），取决于实现 */
    if (buf) {
        buf_unpin(buf);
    }
}

TEST_F(StorageIntegrationTest, DuplicateTable) {
    column_def_t cols[1];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;

    /* 创建第一个表 */
    Oid oid1 = catalog_create_table("dup_test", cols, 1);
    ASSERT_NE(InvalidOid, oid1);

    /* 尝试创建同名表应该失败 */
    Oid oid2 = catalog_create_table("dup_test", cols, 1);
    EXPECT_EQ(InvalidOid, oid2);
}
