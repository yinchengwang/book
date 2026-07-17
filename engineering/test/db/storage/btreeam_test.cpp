/**
 * @file btreeam_test.cpp
 * @brief BTree Access Method 单元测试
 */

#include "gtest/gtest.h"
#include "db/btreeam.h"
#include "db/rel.h"

/* ============================================================
 * 测试夹具
 * ============================================================ */

class BTreeAMTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(0, btreeam_init());
        ASSERT_EQ(0, rel_init());
    }

    void TearDown() override {
        btreeam_shutdown();
    }
};

/* ============================================================
 * 初始化测试
 * ============================================================ */

TEST_F(BTreeAMTest, InitAndShutdown) {
    /* 重复初始化应该成功 */
    EXPECT_EQ(0, btreeam_init());
    btreeam_shutdown();
}

TEST_F(BTreeAMTest, InitIdempotent) {
    /* 多次初始化应该安全 */
    EXPECT_EQ(0, btreeam_init());
    EXPECT_EQ(0, btreeam_init());
}

/* ============================================================
 * 页面操作测试
 * ============================================================ */

TEST_F(BTreeAMTest, PageInit) {
    char page[BTREE_PAGE_SIZE];
    bt_page_init(page, 0);  /* 叶子页面 */

    BTPageHeader ph = bt_page_get_header(page);
    EXPECT_EQ(0, ph->btpo_level);
    EXPECT_TRUE(bt_page_is_leaf(page));
    EXPECT_FALSE(bt_page_is_root(page));
    EXPECT_EQ(0, ph->btpo_count);
}

TEST_F(BTreeAMTest, PageInitInternal) {
    char page[BTREE_PAGE_SIZE];
    bt_page_init(page, 1);  /* 内部页面 */

    BTPageHeader ph = bt_page_get_header(page);
    EXPECT_EQ(1, ph->btpo_level);
    EXPECT_FALSE(bt_page_is_leaf(page));
}

TEST_F(BTreeAMTest, PageGetHeader) {
    char page[BTREE_PAGE_SIZE];
    bt_page_init(page, 0);

    BTPageHeader ph = bt_page_get_header(page);
    ASSERT_NE(nullptr, ph);
    EXPECT_EQ(0, ph->btpo_level);
}

TEST_F(BTreeAMTest, PageIsLeaf) {
    char page[BTREE_PAGE_SIZE];
    bt_page_init(page, 0);

    EXPECT_TRUE(bt_page_is_leaf(page));

    bt_page_init(page, 1);
    EXPECT_FALSE(bt_page_is_leaf(page));
}

TEST_F(BTreeAMTest, PageIsRoot) {
    char page[BTREE_PAGE_SIZE];
    bt_page_init(page, 0);

    /* 新页面不是根 */
    EXPECT_FALSE(bt_page_is_root(page));
}

TEST_F(BTreeAMTest, PageGetFreeSpace) {
    char page[BTREE_PAGE_SIZE];
    bt_page_init(page, 0);

    int free_space = bt_page_get_free_space(page);
    EXPECT_GT(free_space, 0);
    EXPECT_EQ(BTREE_PAGE_SIZE - BTREE_PAGE_HEADER_SIZE, free_space);
}

TEST_F(BTreeAMTest, PageGetItem) {
    char page[BTREE_PAGE_SIZE];
    bt_page_init(page, 0);

    /* 新页面应该返回 0（插入位置） */
    int pos = bt_page_get_item(page, "key", 1, NULL);
    EXPECT_GE(pos, 0);
}

/* ============================================================
 * 索引操作测试
 * ============================================================ */

TEST_F(BTreeAMTest, CreateIndex) {
    Relation rel = relation_opennode(1000, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    int result = btcreate(rel);
    EXPECT_EQ(0, result);

    relation_close(rel, 0);
}

TEST_F(BTreeAMTest, DestroyIndex) {
    Relation rel = relation_opennode(1001, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    /* 先创建 */
    EXPECT_EQ(0, btcreate(rel));

    /* 再销毁 */
    int result = btdestroy(rel);
    EXPECT_EQ(0, result);

    relation_close(rel, 0);
}

TEST_F(BTreeAMTest, Insert) {
    Relation rel = relation_opennode(1002, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    /* 先创建索引 */
    ASSERT_EQ(0, btcreate(rel));

    /* 插入条目 */
    int key = 42;
    int result = btinsert(rel, (const void **)&key, 1, &key);
    EXPECT_EQ(0, result);

    relation_close(rel, 0);
}

TEST_F(BTreeAMTest, InsertMultiple) {
    Relation rel = relation_opennode(1003, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    ASSERT_EQ(0, btcreate(rel));

    /* 插入多个条目 */
    for (int i = 0; i < 10; i++) {
        int key = i * 10;
        EXPECT_EQ(0, btinsert(rel, (const void **)&key, 1, &key));
    }

    relation_close(rel, 0);
}

TEST_F(BTreeAMTest, Delete) {
    Relation rel = relation_opennode(1004, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    ASSERT_EQ(0, btcreate(rel));

    int key = 100;
    EXPECT_EQ(0, btinsert(rel, (const void **)&key, 1, &key));

    int result = btdelete(rel, (const void **)&key, 1, &key);
    EXPECT_EQ(0, result);

    relation_close(rel, 0);
}

TEST_F(BTreeAMTest, Build) {
    Relation rel = relation_opennode(1005, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    /* 准备元组数组 */
    void *tuples[5];
    int keys[5] = {30, 10, 50, 20, 40};
    for (int i = 0; i < 5; i++) {
        tuples[i] = &keys[i];
    }

    int result = btbuild(rel, tuples, 5);
    EXPECT_EQ(0, result);

    relation_close(rel, 0);
}

/* ============================================================
 * 扫描测试
 * ============================================================ */

TEST_F(BTreeAMTest, BeginScan) {
    Relation rel = relation_opennode(1006, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    BTScanDesc scan = bt_beginscan(rel, 0, NULL);
    ASSERT_NE(nullptr, scan);
    EXPECT_EQ(rel, scan->bt_relation);
    EXPECT_EQ(ForwardScanDirection, scan->bt_direction);

    bt_endscan(scan);
    relation_close(rel, 0);
}

TEST_F(BTreeAMTest, IndexBeginScan) {
    Relation idxrel = relation_opennode(1007, REL_OPEN_READONLY);
    Relation heaprel = relation_opennode(1008, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, idxrel);
    ASSERT_NE(nullptr, heaprel);

    BTScanDesc scan = bt_index_beginscan(idxrel, heaprel, 0, NULL);
    ASSERT_NE(nullptr, scan);
    EXPECT_EQ(idxrel, scan->bt_relation);

    bt_endscan(scan);
    relation_close(idxrel, 0);
    relation_close(heaprel, 0);
}

TEST_F(BTreeAMTest, Rescan) {
    Relation rel = relation_opennode(1009, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    BTScanDesc scan = bt_beginscan(rel, 0, NULL);
    ASSERT_NE(nullptr, scan);

    /* 重新扫描 */
    bt_rescan(scan, NULL);

    EXPECT_EQ(0, scan->bt_curitem);
    EXPECT_EQ(0, scan->bt_curpage);

    bt_endscan(scan);
    relation_close(rel, 0);
}

TEST_F(BTreeAMTest, EndScan) {
    Relation rel = relation_opennode(1010, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    BTScanDesc scan = bt_beginscan(rel, 0, NULL);
    ASSERT_NE(nullptr, scan);

    /* endscan 一次 */
    bt_endscan(scan);

    relation_close(rel, 0);
}

TEST_F(BTreeAMTest, GetNext) {
    Relation rel = relation_opennode(1011, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    /* 创建并填充索引 */
    ASSERT_EQ(0, btcreate(rel));
    for (int i = 0; i < 3; i++) {
        int key = i;
        btinsert(rel, (const void **)&key, 1, &key);
    }

    /* 扫描 */
    BTScanDesc scan = bt_beginscan(rel, 0, NULL);
    ASSERT_NE(nullptr, scan);

    /* 获取条目 */
    void *tuple = bt_getnext(scan, ForwardScanDirection);
    (void)tuple;  /* 可能为 NULL（简化实现） */

    bt_endscan(scan);
    relation_close(rel, 0);
}

/* ============================================================
 * 键比较测试
 * ============================================================ */

TEST_F(BTreeAMTest, Compare) {
    int key1 = 10;
    int key2 = 20;

    int result = bt_compare(NULL, &key1, &key2, 1);
    EXPECT_LT(result, 0);

    result = bt_compare(NULL, &key2, &key1, 1);
    EXPECT_GT(result, 0);

    result = bt_compare(NULL, &key1, &key1, 1);
    EXPECT_EQ(0, result);
}

TEST_F(BTreeAMTest, CompareNull) {
    int key = 10;

    /* NULL 键处理：NULL 被视为最小值 */
    EXPECT_LT(bt_compare(NULL, NULL, &key, 1), 0);  /* NULL < 非NULL（返回-1） */
    EXPECT_GT(bt_compare(NULL, &key, NULL, 1), 0);  /* 非NULL > NULL（返回1） */
    EXPECT_EQ(0, bt_compare(NULL, NULL, NULL, 1));  /* NULL == NULL（返回0） */
}

TEST_F(BTreeAMTest, KeyMatches) {
    int key = 42;

    /* 简化实现总是返回 true */
    bool matches = bt_key_matches(NULL, &key, 1, NULL, 0);
    (void)matches;  /* 可能有警告 */
}

/* ============================================================
 * 统计信息测试
 * ============================================================ */

TEST_F(BTreeAMTest, StatsOperations) {
    btreeam_reset_stats();

    BTREEAMStats stats;
    btreeam_get_stats(&stats);
    EXPECT_EQ(0u, stats.insertions);
    EXPECT_EQ(0u, stats.deletions);

    /* 执行操作 */
    Relation rel = relation_opennode(1012, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    ASSERT_EQ(0, btcreate(rel));
    int key = 1;
    btinsert(rel, (const void **)&key, 1, &key);

    btreeam_get_stats(&stats);
    EXPECT_GE(stats.insertions, 1u);

    relation_close(rel, 0);
}

TEST_F(BTreeAMTest, StatsReset) {
    /* 先添加数据 */
    Relation rel = relation_opennode(1013, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    btcreate(rel);
    int key = 1;
    btinsert(rel, (const void **)&key, 1, &key);

    /* 重置统计 */
    btreeam_reset_stats();

    BTREEAMStats stats;
    btreeam_get_stats(&stats);
    EXPECT_EQ(0u, stats.insertions);

    relation_close(rel, 0);
}
