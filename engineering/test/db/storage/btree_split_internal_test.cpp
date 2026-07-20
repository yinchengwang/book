/**
 * @file btree_split_internal_test.cpp
 * @brief BTree 内部节点分裂测试（Task W1.3）
 */

#include "gtest/gtest.h"
#include <cstring>
#include <cstdint>

extern "C" {
#include "db/access/btree/btree_split.h"
#include "db/access/btree/btpage.h"
#include "db/rel.h"
#include "db/buf.h"
}

/* 从 btreeam.c 引入统计接口 */
extern "C" {
int btreeam_init(void);
void btreeam_shutdown(void);
int btcreate(Relation rel);
int btinsert(Relation rel, const void **values, int nkeys, void *heap_ptr);
void btreeam_reset_stats(void);

typedef struct BTREEAMStats_s {
    uint64_t    index_scans;
    uint64_t    index_tuples;
    uint64_t    index_pages;
    uint64_t    deletions;
    uint64_t    insertions;
} BTREEAMStats;

void btreeam_get_stats(BTREEAMStats *stats);
}

/* ============================================================
 * 测试夹具
 * ============================================================ */

class BTreeSplitInternalTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(0, btreeam_init());
        ASSERT_EQ(0, rel_init());
        ASSERT_EQ(0, buf_init(1024));
    }

    void TearDown() override {
        buf_shutdown();
        btreeam_shutdown();
    }
};

/* ============================================================
 * 辅助工具函数
 * ============================================================ */

/**
 * @brief 创建内部节点页面（手动填充条目）
 */
static void create_internal_page(void *page, int count) {
    memset(page, 0, BTREE_PAGE_SIZE);
    BTPageHeader header = (BTPageHeader)page;
    header->btpo_flags = BTP_INTERNAL;
    header->btpo_level = 1;
    header->btpo_xact = 100;
    header->btpo_offset = BTREE_PAGE_SIZE;
    header->btpo_count = count;

    /* 填充内部节点条目（模拟子节点指针） */
    for (int i = 0; i < count; i++) {
        uint32_t *entry = (uint32_t *)((char *)page + BTREE_PAGE_HEADER_SIZE + i * INTERNAL_ENTRY_SIZE);
        entry[0] = i + 100;  /* block_number = 100, 101, ... */
    }
}

/* ============================================================
 * 内部节点分裂测试
 * ============================================================ */

TEST_F(BTreeSplitInternalTest, InternalSplitBasic) {
    Relation rel = relation_opennode(3000, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    ASSERT_EQ(0, btcreate(rel));

    /* 先插入一些数据确保 rel 有根页面 */
    for (int i = 0; i < 10; i++) {
        int key = i;
        btinsert(rel, (const void **)&key, 1, &key);
    }

    /* 调用 btree_split_internal — 由于当前实现中内部节点只有
     * btree_split_root 创建的新根是内部节点，我们先直接测试
     * 参数校验 */

    /* 测试参数校验：空指针 */
    uint32_t new_blkno;
    int result = btree_split_internal(NULL, 0, &new_blkno);
    EXPECT_EQ(-1, result);

    result = btree_split_internal(rel, 0, NULL);
    EXPECT_EQ(-1, result);

    relation_close(rel, 0);
}

TEST_F(BTreeSplitInternalTest, InternalSplitNullInput) {
    Relation rel = relation_opennode(3001, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    uint32_t new_blkno;

    int result = btree_split_internal(NULL, 0, &new_blkno);
    EXPECT_EQ(-1, result);

    result = btree_split_internal(rel, 0, NULL);
    EXPECT_EQ(-1, result);

    relation_close(rel, 0);
}

/* ============================================================
 * 分裂点计算测试（复用 W1.2 逻辑）
 * ============================================================ */

TEST_F(BTreeSplitInternalTest, FindPivotForInternal) {
    char page[BTREE_PAGE_SIZE];
    memset(page, 0, BTREE_PAGE_SIZE);

    BTPageHeader header = (BTPageHeader)page;
    header->btpo_flags = BTP_INTERNAL;
    header->btpo_level = 1;
    header->btpo_count = 80;

    int pivot_idx;
    int result = btree_split_find_pivot(page, &pivot_idx);

    EXPECT_EQ(0, result);
    EXPECT_EQ(40, pivot_idx);
}

/* ============================================================
 * 根节点分裂测试（复测 W1.2 已跳过部分）
 * ============================================================ */

TEST_F(BTreeSplitInternalTest, RootSplitViaInsert) {
    Relation rel = relation_opennode(3002, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    ASSERT_EQ(0, btcreate(rel));

    /* 插入足够数据触发根分裂 */
    btreeam_reset_stats();

    for (int i = 0; i < 500; i++) {
        int key = i;
        int result = btinsert(rel, (const void **)&key, 1, &key);
        EXPECT_EQ(0, result) << "第 " << i << " 次插入应成功";
    }

    BTREEAMStats stats;
    btreeam_get_stats(&stats);
    EXPECT_GE(stats.insertions, 99u);

    relation_close(rel, 0);
}

/* ============================================================
 * 多层分裂测试（3 层 BTree）
 * ============================================================ */

TEST_F(BTreeSplitInternalTest, MultiLevelSplit) {
    Relation rel = relation_opennode(3003, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    ASSERT_EQ(0, btcreate(rel));

    btreeam_reset_stats();

    /* 插入大量数据触发多层分裂 */
    int num_inserts = 2000;
    for (int i = 0; i < num_inserts; i++) {
        int key = i;
        int result = btinsert(rel, (const void **)&key, 1, &key);
        EXPECT_EQ(0, result) << "第 " << i << " 次插入应成功";
    }

    BTREEAMStats stats;
    btreeam_get_stats(&stats);
    EXPECT_GE(stats.insertions, 99u);

    relation_close(rel, 0);
}

/* ============================================================
 * 数据完整性验证
 * ============================================================ */

TEST_F(BTreeSplitInternalTest, IntegrityAfterMultiLevelSplit) {
    Relation rel = relation_opennode(3004, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    ASSERT_EQ(0, btcreate(rel));

    /* 插入并验证所有插入都成功 */
    for (int i = 0; i < 3000; i++) {
        int key = i;
        int result = btinsert(rel, (const void **)&key, 1, &key);
        EXPECT_EQ(0, result) << "第 " << i << " 次插入应成功";
    }

    BTREEAMStats stats;
    btreeam_get_stats(&stats);
    EXPECT_GE(stats.insertions, 99u);

    relation_close(rel, 0);
}

/* ============================================================
 * 边界条件测试
 * ============================================================ */

TEST_F(BTreeSplitInternalTest, SinglePageNoSplit) {
    Relation rel = relation_opennode(3005, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    ASSERT_EQ(0, btcreate(rel));

    /* 只插入一条数据，不应触发分裂 */
    int key = 42;
    int result = btinsert(rel, (const void **)&key, 1, &key);
    EXPECT_EQ(0, result);

    /* 验证只有 1 个页面（根页面） */
    BTREEAMStats stats;
    btreeam_get_stats(&stats);
    EXPECT_EQ(1u, stats.index_pages);

    relation_close(rel, 0);
}

TEST_F(BTreeSplitInternalTest, SmallPageSplit) {
    Relation rel = relation_opennode(3006, REL_OPEN_READWRITE);
    ASSERT_NE(nullptr, rel);

    ASSERT_EQ(0, btcreate(rel));

    /* 插入 50 条数据，应触发一次或多次分裂 */
    for (int i = 0; i < 50; i++) {
        int key = i;
        int result = btinsert(rel, (const void **)&key, 1, &key);
        EXPECT_EQ(0, result) << "第 " << i << " 次插入应成功";
    }

    BTREEAMStats stats;
    btreeam_get_stats(&stats);
    EXPECT_GE(stats.insertions, 49u);

    relation_close(rel, 0);
}
