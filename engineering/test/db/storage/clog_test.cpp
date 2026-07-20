/**
 * @file clog_test.cpp
 * @brief CLOG 持久化测试（Task W1.5）
 */

#include "gtest/gtest.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "db/storage/txn/clog.h"
}

/* ============================================================
 * 测试辅助：递归删除目录（跨平台）
 * ============================================================ */

static void remove_dir_recursive(const char *path) {
    /* 先尝试 rmdir（空目录） */
    rmdir(path);
}

/* ============================================================
 * 测试夹具
 * ============================================================ */

class ClogTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 清理上次测试残留 */
        remove_dir_recursive("./test_clog_data/pg_xact");
        remove_dir_recursive("./test_clog_data");
        ASSERT_EQ(0, clog_init("./test_clog_data"));
        clog_reset_stats();
    }

    void TearDown() override {
        clog_shutdown();
        /* 清理测试数据 */
        remove_dir_recursive("./test_clog_data/pg_xact");
        remove_dir_recursive("./test_clog_data");
    }
};

/* ============================================================
 * 基础功能测试
 * ============================================================ */

TEST_F(ClogTest, SetAndGet) {
    /* 设置并获取单个事务状态 */
    ASSERT_EQ(0, clog_set_status(100, CLOG_STATUS_COMMITTED));
    EXPECT_EQ(CLOG_STATUS_COMMITTED, clog_get_status(100));

    ASSERT_EQ(0, clog_set_status(200, CLOG_STATUS_ABORTED));
    EXPECT_EQ(CLOG_STATUS_ABORTED, clog_get_status(200));

    ASSERT_EQ(0, clog_set_status(300, CLOG_STATUS_IN_PROGRESS));
    EXPECT_EQ(CLOG_STATUS_IN_PROGRESS, clog_get_status(300));
}

TEST_F(ClogTest, SetAndGetLargeXid) {
    /* 测试跨越段文件边界的大 XID（每个段覆盖 256KB * 4 = 1M 事务） */
    uint32_t base = CLOG_XACTS_PER_SEG * 5;  /* 第 5 个段 */
    ASSERT_EQ(0, clog_set_status(base, CLOG_STATUS_COMMITTED));
    EXPECT_EQ(CLOG_STATUS_COMMITTED, clog_get_status(base));

    ASSERT_EQ(0, clog_set_status(base + 1000, CLOG_STATUS_ABORTED));
    EXPECT_EQ(CLOG_STATUS_ABORTED, clog_get_status(base + 1000));
}

TEST_F(ClogTest, MultipleXidsSameByte) {
    /* 测试同一字节内的 4 个事务（每个事务占 2bit） */
    uint32_t base = 1000;
    /* 1000, 1001, 1002, 1003 应该在同一字节内 */
    clog_set_status(base,     CLOG_STATUS_IN_PROGRESS);
    clog_set_status(base + 1, CLOG_STATUS_COMMITTED);
    clog_set_status(base + 2, CLOG_STATUS_ABORTED);
    clog_set_status(base + 3, CLOG_STATUS_COMMITTED);

    EXPECT_EQ(CLOG_STATUS_IN_PROGRESS, clog_get_status(base));
    EXPECT_EQ(CLOG_STATUS_COMMITTED,   clog_get_status(base + 1));
    EXPECT_EQ(CLOG_STATUS_ABORTED,    clog_get_status(base + 2));
    EXPECT_EQ(CLOG_STATUS_COMMITTED,   clog_get_status(base + 3));
}

TEST_F(ClogTest, DefaultStatus) {
    /* 未设置的事务应返回 IN_PROGRESS */
    EXPECT_EQ(CLOG_STATUS_IN_PROGRESS, clog_get_status(999999));
    EXPECT_EQ(CLOG_STATUS_IN_PROGRESS, clog_get_status(888888));
}

/* ============================================================
 * 持久化测试
 * ============================================================ */

TEST_F(ClogTest, PersistenceAfterFlush) {
    /* 设置状态并刷盘，然后重启读取 */
    ASSERT_EQ(0, clog_set_status(500, CLOG_STATUS_COMMITTED));
    ASSERT_EQ(0, clog_set_status(501, CLOG_STATUS_ABORTED));
    clog_flush();
    clog_shutdown();

    /* 重启后重新初始化 */
    ASSERT_EQ(0, clog_init("./test_clog_data"));
    EXPECT_EQ(CLOG_STATUS_COMMITTED, clog_get_status(500));
    EXPECT_EQ(CLOG_STATUS_ABORTED,   clog_get_status(501));
}

TEST_F(ClogTest, PartialFlush) {
    /* 部分刷盘（只刷某个段） */
    int segno = clog_get_segno(1000);
    clog_set_status(1000, CLOG_STATUS_COMMITTED);
    clog_set_status(1001, CLOG_STATUS_ABORTED);

    /* 刷指定段 */
    clog_flush_segment(segno);
    clog_shutdown();

    ASSERT_EQ(0, clog_init("./test_clog_data"));
    EXPECT_EQ(CLOG_STATUS_COMMITTED, clog_get_status(1000));
    EXPECT_EQ(CLOG_STATUS_ABORTED,   clog_get_status(1001));
}

/* ============================================================
 * 边界条件测试
 * ============================================================ */

TEST_F(ClogTest, SegmentBoundary) {
    /* 测试段边界附近的事务 */
    uint32_t boundary = CLOG_XACTS_PER_SEG;  /* 段的起始 */
    ASSERT_EQ(0, clog_set_status(boundary, CLOG_STATUS_COMMITTED));
    EXPECT_EQ(CLOG_STATUS_COMMITTED, clog_get_status(boundary));

    /* 段末尾 */
    uint32_t last_in_seg = boundary - 1;
    ASSERT_EQ(0, clog_set_status(last_in_seg, CLOG_STATUS_ABORTED));
    EXPECT_EQ(CLOG_STATUS_ABORTED, clog_get_status(last_in_seg));
}

TEST_F(ClogTest, LargeXidRange) {
    /* 跨多个段的事务 */
    for (uint32_t xid = 0; xid < CLOG_XACTS_PER_SEG * 3; xid += 1000) {
        uint8_t status = (xid % 3 == 0) ? CLOG_STATUS_COMMITTED :
                         (xid % 3 == 1) ? CLOG_STATUS_ABORTED : CLOG_STATUS_IN_PROGRESS;
        ASSERT_EQ(0, clog_set_status(xid, status));
        EXPECT_EQ(status, clog_get_status(xid));
    }
}

/* ============================================================
 * 统计测试
 * ============================================================ */

TEST_F(ClogTest, Stats) {
    clog_set_status(100, CLOG_STATUS_COMMITTED);
    clog_get_status(100);
    clog_get_status(200);  /* 未设置，触发缓存未命中 */
    clog_flush();

    ClogStats stats;
    clog_get_stats(&stats);

    EXPECT_GE(stats.status_sets, 1u);
    EXPECT_GE(stats.status_gets, 2u);
}

TEST_F(ClogTest, CacheHit) {
    /* 连续读取同一事务，第二次应命中缓存 */
    clog_get_status(1000);
    clog_reset_stats();

    /* 再读一次，缓存应命中 */
    for (int i = 0; i < 10; i++) {
        clog_get_status(1000);
    }

    ClogStats stats;
    clog_get_stats(&stats);
    /* 缓存应命中 9 次（第一次后 reset，之后读了 10 次） */
    EXPECT_GE(stats.cache_hits, 9u);
}
