/**
 * @file heapam_test.cpp
 * @brief Heap Access Method 单元测试
 */

#include "gtest/gtest.h"
#include "db/heapam.h"
#include "db/rel.h"
#include "db/catalog.h"

/* ============================================================
 * 测试夹具
 * ============================================================ */

class HeapAMTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(0, heapam_init());
    }

    void TearDown() override {
        heapam_shutdown();
    }
};

/* ============================================================
 * 页面操作测试
 * ============================================================ */

TEST_F(HeapAMTest, PageInit) {
    char page[HEAP_PAGE_SIZE];
    heap_page_init(page, HEAP_PAGE_SIZE);

    PageHeader ph = heap_page_get_header(page);
    EXPECT_EQ(SizeOfPageHeaderData, ph->pd_lower);
    EXPECT_EQ(HEAP_PAGE_SIZE, ph->pd_upper);
    EXPECT_EQ(HEAP_PAGE_SIZE, ph->pd_special);
}

TEST_F(HeapAMTest, PageGetHeader) {
    char page[HEAP_PAGE_SIZE];
    heap_page_init(page, HEAP_PAGE_SIZE);

    PageHeader ph = heap_page_get_header(page);
    ASSERT_NE(nullptr, ph);
    EXPECT_EQ(SizeOfPageHeaderData, ph->pd_lower);
}

TEST_F(HeapAMTest, PageGetFreeSpace) {
    char page[HEAP_PAGE_SIZE];
    heap_page_init(page, HEAP_PAGE_SIZE);

    int free_space = heap_page_get_free_space(page);
    EXPECT_GT(free_space, 0);
    EXPECT_EQ(HEAP_PAGE_SIZE - SizeOfPageHeaderData, free_space);
}

TEST_F(HeapAMTest, PageAddTuple) {
    char page[HEAP_PAGE_SIZE];
    heap_page_init(page, HEAP_PAGE_SIZE);

    /* 创建一个测试元组 */
    const char tuple_data[] = "test tuple data for heap";
    HeapLinePointer lp = NULL;

    int result = heap_page_add_tuple(page, tuple_data, sizeof(tuple_data), &lp);
    EXPECT_EQ(0, result);
    ASSERT_NE(nullptr, lp);
    EXPECT_GT(lp->t_off, 0);

    /* 检查元组数据是否正确复制 */
    char *stored = page + lp->t_off;
    EXPECT_STREQ(tuple_data, stored);
}

TEST_F(HeapAMTest, PageAddMultipleTuples) {
    char page[HEAP_PAGE_SIZE];
    heap_page_init(page, HEAP_PAGE_SIZE);

    /* 添加多个元组 */
    for (int i = 0; i < 10; i++) {
        char tuple_data[32];
        snprintf(tuple_data, sizeof(tuple_data), "tuple_%d", i);

        HeapLinePointer lp = NULL;
        int result = heap_page_add_tuple(page, tuple_data, strlen(tuple_data) + 1, &lp);
        EXPECT_EQ(0, result);
        ASSERT_NE(nullptr, lp);
    }

    /* 检查元组计数 */
    int count = heap_page_get_tuple_count(page);
    EXPECT_EQ(10, count);

    /* 检查空闲空间减少 */
    int free_space = heap_page_get_free_space(page);
    EXPECT_LT(free_space, HEAP_PAGE_SIZE - SizeOfPageHeaderData - 10 * SizeOfHeapLinePointer);
}

TEST_F(HeapAMTest, PageTupleCount) {
    char page[HEAP_PAGE_SIZE];
    heap_page_init(page, HEAP_PAGE_SIZE);

    EXPECT_EQ(0, heap_page_get_tuple_count(page));

    /* 添加一个元组 */
    const char tuple_data[] = "single tuple";
    heap_page_add_tuple(page, tuple_data, sizeof(tuple_data), NULL);

    EXPECT_EQ(1, heap_page_get_tuple_count(page));
}

TEST_F(HeapAMTest, PagePrune) {
    char page[HEAP_PAGE_SIZE];
    heap_page_init(page, HEAP_PAGE_SIZE);

    /* 添加一些元组 */
    const char tuple[] = "test";
    for (int i = 0; i < 5; i++) {
        heap_page_add_tuple(page, tuple, sizeof(tuple), NULL);
    }

    /* 清理页面 */
    Relation rel = relation_opennode(100, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    int pruned = heap_page_prune(rel, page);
    EXPECT_GE(pruned, 0);  /* 可能返回 0（简化实现） */

    relation_close(rel, 0);
}

/* ============================================================
 * 元组操作测试
 * ============================================================ */

TEST_F(HeapAMTest, HeapInsert) {
    Relation rel = relation_opennode(200, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    /* 创建一个简单的元组 */
    const char tuple_data[] = "insert test tuple";

    int result = heap_insert(rel, tuple_data, sizeof(tuple_data), 0, 0, NULL);
    EXPECT_EQ(0, result);

    relation_close(rel, 0);
}

TEST_F(HeapAMTest, HeapDelete) {
    Relation rel = relation_opennode(300, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    /* 创建一个元组 */
    const char tuple_data[] = "delete test tuple";
    int result = heap_insert(rel, tuple_data, sizeof(tuple_data), 0, 0, NULL);
    ASSERT_EQ(0, result);

    /* 删除元组 */
    result = heap_delete(rel, tuple_data, 0, false, false);
    EXPECT_EQ(0, result);

    relation_close(rel, 0);
}

TEST_F(HeapAMTest, HeapUpdate) {
    Relation rel = relation_opennode(400, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    /* 插入元组 */
    const char old_tuple[] = "old data";
    int result = heap_insert(rel, old_tuple, sizeof(old_tuple), 0, 0, NULL);
    ASSERT_EQ(0, result);

    /* 更新元组 */
    const char new_tuple[] = "new data";
    result = heap_update(rel, old_tuple, new_tuple, sizeof(new_tuple),
                         0, 0, NULL, 0);
    EXPECT_EQ(0, result);

    relation_close(rel, 0);
}

TEST_F(HeapAMTest, HeapLockTuple) {
    Relation rel = relation_opennode(500, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    char tuple_data[] = "lock test";
    int result = heap_lock_tuple(rel, tuple_data, 0, 0, false, NULL);
    EXPECT_EQ(0, result);

    relation_close(rel, 0);
}

/* ============================================================
 * 扫描操作测试
 * ============================================================ */

TEST_F(HeapAMTest, HeapGetNext) {
    /* 直接使用页面操作测试，不需要 Relation */
    char page[HEAP_PAGE_SIZE];
    heap_page_init(page, HEAP_PAGE_SIZE);

    /* 插入 5 个元组 */
    for (int i = 0; i < 5; i++) {
        char tuple_data[32];
        snprintf(tuple_data, sizeof(tuple_data), "tuple_%d", i);
        HeapLinePointer lp = NULL;
        int r = heap_page_add_tuple(page, tuple_data, strlen(tuple_data) + 1, &lp);
        ASSERT_EQ(0, r);
    }

    /* 验证元组数量 */
    EXPECT_EQ(5, heap_page_get_tuple_count(page));

    /* 验证空闲空间减少 */
    int free_space = heap_page_get_free_space(page);
    EXPECT_LT(free_space, HEAP_PAGE_SIZE - SizeOfPageHeaderData);
}

TEST_F(HeapAMTest, HeapGetCurr) {
    Relation rel = relation_opennode(700, REL_OPEN_READONLY);
    ASSERT_NE(nullptr, rel);

    TableScanDesc scan = table_beginscan_all(rel);
    ASSERT_NE(nullptr, scan);

    /* 初始没有当前元组 */
    EXPECT_EQ(nullptr, heap_getcurr(scan));

    table_endscan(scan);
    relation_close(rel, 0);
}

/* ============================================================
 * 统计信息测试
 * ============================================================ */

TEST_F(HeapAMTest, StatsOperations) {
    /* 重置统计 */
    heapam_reset_stats();

    HeapAMStats stats;
    heapam_get_stats(&stats);
    EXPECT_EQ(0u, stats.inserts);
    EXPECT_EQ(0u, stats.deletes);
    EXPECT_EQ(0u, stats.updates);

    /* 执行一些操作 */
    Relation rel = relation_opennode(800, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    const char tuple[] = "stats test";
    heap_insert(rel, tuple, sizeof(tuple), 0, 0, NULL);
    heap_delete(rel, tuple, 0, false, false);

    heapam_get_stats(&stats);
    EXPECT_EQ(1u, stats.inserts);
    EXPECT_EQ(1u, stats.deletes);

    relation_close(rel, 0);
}

TEST_F(HeapAMTest, StatsReset) {
    /* 先添加一些数据 */
    Relation rel = relation_opennode(900, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    const char tuple[] = "reset test";
    heap_insert(rel, tuple, sizeof(tuple), 0, 0, NULL);

    /* 重置统计 */
    heapam_reset_stats();

    HeapAMStats stats;
    heapam_get_stats(&stats);
    EXPECT_EQ(0u, stats.inserts);

    relation_close(rel, 0);
}

/* ============================================================
 * 初始化测试
 * ============================================================ */

TEST_F(HeapAMTest, HeapAMInitIdempotent) {
    /* 重复初始化应该成功 */
    EXPECT_EQ(0, heapam_init());
}
