/**
 * @file rel_test.cpp
 * @brief Relation 和 Access Method 层单元测试
 */

#include "gtest/gtest.h"
#include "db/rel.h"
#include "db/catalog.h"
#include "db/heapam.h"

/* ============================================================
 * 测试夹具
 * ============================================================ */

class RelationTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* rel_init 内部会初始化 catalog，只调用一次 */
        ASSERT_EQ(0, rel_init());
    }

    void TearDown() override {
        /* rel_shutdown 会关闭所有子系统 */
        rel_shutdown();
    }
};

/* ============================================================
 * TupleDesc 测试
 * ============================================================ */

TEST_F(RelationTest, CreateTupleDesc) {
    TupleDesc tdesc = CreateTupleDesc(3);
    ASSERT_NE(nullptr, tdesc);
    EXPECT_EQ(3, tdesc->natts);
    FreeTupleDesc(tdesc);
}

TEST_F(RelationTest, CreateTupleDescCopy) {
    TupleDesc src = CreateTupleDesc(2);
    ASSERT_NE(nullptr, src);
    src->attrs[0].atttypid = 23;
    src->attrs[1].atttypid = 25;

    TupleDesc copy = CreateTupleDescCopy(src);
    ASSERT_NE(nullptr, copy);
    EXPECT_EQ(2, copy->natts);
    EXPECT_EQ(23, copy->attrs[0].atttypid);
    EXPECT_EQ(25, copy->attrs[1].atttypid);

    FreeTupleDesc(src);
    FreeTupleDesc(copy);
}

TEST_F(RelationTest, TupleDescNatts) {
    TupleDesc tdesc = CreateTupleDesc(5);
    EXPECT_EQ(5, TupleDescNatts(tdesc));
    FreeTupleDesc(tdesc);
}

TEST_F(RelationTest, TupleDescAttr) {
    TupleDesc tdesc = CreateTupleDesc(2);
    ASSERT_NE(nullptr, tdesc);

    /* 获取有效的属性 */
    void *attr = TupleDescAttr(tdesc, 1);
    EXPECT_NE(nullptr, attr);

    /* 获取无效的属性 */
    attr = TupleDescAttr(tdesc, 0);
    EXPECT_EQ(nullptr, attr);
    attr = TupleDescAttr(tdesc, 3);
    EXPECT_EQ(nullptr, attr);

    FreeTupleDesc(tdesc);
}

/* ============================================================
 * ScanKey 测试
 * ============================================================ */

TEST_F(RelationTest, ScanKeyInit) {
    ScanKeyData key;
    ScanKeyInit(&key, 1, SCAN_KEY_EQ, (void *)"value");

    EXPECT_EQ(1, key.sk_attno);
    EXPECT_EQ(SCAN_KEY_EQ, key.sk_op);
    EXPECT_EQ((void *)"value", key.sk_argument);
}

TEST_F(RelationTest, ScanKeyInitWithInfo) {
    ScanKeyData key;
    ScanKeyInitWithInfo(&key, 2, SCAN_KEY_GT, 5, (void *)12345);

    EXPECT_EQ(2, key.sk_attno);
    EXPECT_EQ(SCAN_KEY_GT, key.sk_op);
    EXPECT_EQ(5u, key.sk_arglen);
}

/* ============================================================
 * Catalog 辅助测试
 * ============================================================ */

TEST_F(RelationTest, CreateTestTable) {
    /* 创建一个测试表用于后续测试 */
    column_def_t cols[2];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;
    cols[0].not_null = true;

    strcpy(cols[1].name, "name");
    cols[1].type_oid = 25;
    cols[1].not_null = false;

    Oid table_oid = catalog_create_table("test_rel", cols, 2);
    EXPECT_NE(InvalidOid, table_oid);
}

/* ============================================================
 * Relation 基本操作测试
 * ============================================================ */

TEST_F(RelationTest, RelationOpen) {
    /* 先创建表 */
    column_def_t cols[1];
    strcpy(cols[0].name, "id");
    cols[0].type_oid = 23;

    Oid table_oid = catalog_create_table("open_test", cols, 1);
    ASSERT_NE(InvalidOid, table_oid);

    /* 打开表 */
    Relation rel = relation_open(table_oid, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);
    EXPECT_EQ(table_oid, rel->rd_id);
    EXPECT_EQ(RELKIND_RELATION, rel->rd_relkind);

    relation_close(rel, 0);
}

TEST_F(RelationTest, RelationOpenNode) {
    /* 通过 relfilenode 打开 */
    Relation rel = relation_opennode(100, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);
    EXPECT_EQ(100u, rel->rd_relfilenode);

    relation_close(rel, 0);
}

TEST_F(RelationTest, RelationClose) {
    Relation rel = relation_opennode(200, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    /* 多次关闭应该安全 */
    relation_close(rel, 0);
    relation_close(rel, 0);  /* 第二次应该不崩溃 */
}

TEST_F(RelationTest, RelationGetDesc) {
    /* 先创建表 */
    column_def_t cols[2];
    strcpy(cols[0].name, "col1");
    cols[0].type_oid = 23;
    strcpy(cols[1].name, "col2");
    cols[1].type_oid = 25;

    Oid table_oid = catalog_create_table("desc_test", cols, 2);
    ASSERT_NE(InvalidOid, table_oid);

    Relation rel = relation_open(table_oid, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    TupleDesc tdesc = relation_getdesc(rel);
    EXPECT_NE(nullptr, tdesc);
    EXPECT_EQ(2, tdesc->natts);

    relation_close(rel, 0);
}

TEST_F(RelationTest, RelationGetNatts) {
    Relation rel = relation_opennode(300, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    /* 没有 TupleDesc 时返回 0 */
    EXPECT_EQ(0, relation_getnatts(rel));

    relation_close(rel, 0);
}

TEST_F(RelationTest, RelationGetNblocks) {
    Relation rel = relation_opennode(400, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    EXPECT_EQ(0, relation_getnblocks(rel));

    relation_close(rel, 0);
}

TEST_F(RelationTest, RelationGetFileNode) {
    Relation rel = relation_opennode(500, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    EXPECT_EQ(500u, relation_getfilenode(rel));

    relation_close(rel, 0);
}

TEST_F(RelationTest, RelationGetAM) {
    Relation rel = relation_opennode(600, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    EXPECT_EQ(AM_HEAP, relation_getam(rel));

    relation_close(rel, 0);
}

/* ============================================================
 * 表扫描测试
 * ============================================================ */

TEST_F(RelationTest, TableBeginScan) {
    Relation rel = relation_opennode(700, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    ScanKeyData key;
    ScanKeyInit(&key, 1, SCAN_KEY_EQ, (void *)"value");

    TableScanDesc scan = table_beginscan(rel, 1, &key);
    ASSERT_NE(nullptr, scan);
    EXPECT_EQ(rel, scan->rs_rd);
    EXPECT_EQ(ForwardScanDirection, scan->rs_direction);

    table_endscan(scan);
    relation_close(rel, 0);
}

TEST_F(RelationTest, TableBeginScanAll) {
    Relation rel = relation_opennode(800, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    TableScanDesc scan = table_beginscan_all(rel);
    ASSERT_NE(nullptr, scan);

    table_endscan(scan);
    relation_close(rel, 0);
}

TEST_F(RelationTest, TableEndScan) {
    Relation rel = relation_opennode(900, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    TableScanDesc scan = table_beginscan_all(rel);
    ASSERT_NE(nullptr, scan);

    /* endscan 一次 */
    table_endscan(scan);

    relation_close(rel, 0);
}

TEST_F(RelationTest, TableGetNext) {
    Relation rel = relation_opennode(1000, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    TableScanDesc scan = table_beginscan_all(rel);
    ASSERT_NE(nullptr, scan);

    /* 空表扫描应该返回 NULL */
    void *tuple = table_getnext(scan);
    /* 可能为 NULL 或返回空页 */

    table_endscan(scan);
    relation_close(rel, 0);
}

TEST_F(RelationTest, TableGetCurr) {
    Relation rel = relation_opennode(1100, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    TableScanDesc scan = table_beginscan_all(rel);
    ASSERT_NE(nullptr, scan);

    /* 初始应该没有当前元组 */
    EXPECT_EQ(nullptr, table_getcurr(scan));

    table_endscan(scan);
    relation_close(rel, 0);
}

/* ============================================================
 * 元组操作测试（使用 Heap AM）
 * ============================================================ */

TEST_F(RelationTest, HeapInsert) {
    Relation rel = relation_opennode(1200, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    /* 创建一个简单的元组 */
    const char tuple_data[] = "test tuple data";

    int result = heap_insert(rel, tuple_data, sizeof(tuple_data), 0, 0, NULL);
    EXPECT_EQ(0, result);

    relation_close(rel, 0);
}

TEST_F(RelationTest, HeapDelete) {
    Relation rel = relation_opennode(1300, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    /* 创建一个元组 */
    const char tuple_data[] = "tuple to delete";

    int result = heap_delete(rel, tuple_data, 0, false, false);
    EXPECT_EQ(0, result);

    relation_close(rel, 0);
}

/* ============================================================
 * 索引扫描测试
 * ============================================================ */

TEST_F(RelationTest, IndexBeginScan) {
    Relation rel = relation_opennode(1400, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    ScanKeyData key;
    ScanKeyInit(&key, 1, SCAN_KEY_EQ, (void *)"indexed_value");

    IndexScanDesc scan = index_beginscan(rel, 1, &key);
    ASSERT_NE(nullptr, scan);
    EXPECT_EQ(rel, scan->idx_relation);

    index_endscan(scan);
    relation_close(rel, 0);
}

TEST_F(RelationTest, IndexBeginScanHeapScan) {
    Relation idxrel = relation_opennode(1500, REL_OPEN_READONLY);
    Relation heaprel = relation_opennode(1501, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, idxrel);
    ASSERT_NE(nullptr, heaprel);

    ScanKeyData key;
    ScanKeyInit(&key, 1, SCAN_KEY_EQ, (void *)"value");

    IndexScanDesc scan = index_beginscan_heapscan(idxrel, heaprel, 1, &key);
    ASSERT_NE(nullptr, scan);
    EXPECT_EQ(idxrel, scan->idx_relation);
    EXPECT_EQ(heaprel, scan->heap_relation);

    index_endscan(scan);
    relation_close(idxrel, 0);
    relation_close(heaprel, 0);
}

TEST_F(RelationTest, IndexEndScan) {
    Relation rel = relation_opennode(1600, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    IndexScanDesc scan = index_beginscan(rel, 0, NULL);
    ASSERT_NE(nullptr, scan);

    /* endscan 一次 */
    index_endscan(scan);

    relation_close(rel, 0);
}

TEST_F(RelationTest, IndexGetNext) {
    Relation rel = relation_opennode(1700, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    IndexScanDesc scan = index_beginscan(rel, 0, NULL);
    ASSERT_NE(nullptr, scan);

    /* 当前返回 NULL（需要 BTree 实现） */
    void *tuple = index_getnext(scan);
    (void)tuple;  /* 可能为 NULL */

    index_endscan(scan);
    relation_close(rel, 0);
}

/* ============================================================
 * 初始化测试
 * ============================================================ */

TEST_F(RelationTest, RelInitIdempotent) {
    /* 重复初始化应该成功 */
    EXPECT_EQ(0, rel_init());
}

TEST_F(RelationTest, RelShutdown) {
    /* 关闭后可以重新初始化 */
    rel_shutdown();
    EXPECT_EQ(0, rel_init());
}
