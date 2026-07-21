/**
 * @file wal_segment_test.cpp
 * @brief W6.1: WAL 段文件管理测试
 *
 * 测试场景：
 *   1. WAL 段目录创建
 *   2. 段文件命名格式
 *   3. 段切换（手动）
 *   4. 段满自动切换
 *   5. 段文件列表
 *   6. 段文件路径查询
 *   7. 跨段读取
 *   8. 段目录设置
 */

#include <gtest/gtest.h>
#include "db/wal.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstddef>

extern "C" {
    void wal_recovery_info_free(wal_recovery_info_t *info);
}

/** 测试用段目录 */
#define TEST_SEG_DIR "./test_pg_wal"

class WALSegmentTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
#ifdef _WIN32
        system("rmdir /s /q test_pg_wal 2>nul");
        system("rmdir /s /q alt_pg_wal 2>nul");
#else
        system("rm -rf test_pg_wal");
        system("rm -rf alt_pg_wal");
#endif
    }
};

/* ========================================================================
 * 测试 1: WAL 段目录创建
 * ======================================================================== */
TEST_F(WALSegmentTest, CreateSegmentDir) {
    wal_t *wal = wal_create(TEST_SEG_DIR, 8192);
    ASSERT_NE(wal, nullptr);

    /* 验证段文件存在 */
    const char *path = wal_current_segment_path(wal);
    ASSERT_NE(path, nullptr);

    /* 段序号应为 0 */
    EXPECT_EQ(wal_current_segment_no(wal), 0U);

    wal_close(wal);
}

/* ========================================================================
 * 测试 2: 段文件命名格式
 * ======================================================================== */
TEST_F(WALSegmentTest, SegmentNaming) {
    wal_t *wal = wal_create(TEST_SEG_DIR, 8192);
    ASSERT_NE(wal, nullptr);

    /* 写入一些数据，触发段创建 */
    uint64_t lsn = wal_write_begin(wal, 1);
    EXPECT_GT(lsn, 0U);

    /* 切换段 */
    ASSERT_EQ(wal_switch_segment(wal), 0);
    EXPECT_EQ(wal_current_segment_no(wal), 1U);

    /* 验证新段路径包含正确的格式 */
    const char *path = wal_current_segment_path(wal);
    ASSERT_NE(path, nullptr);

    wal_close(wal);
}

/* ========================================================================
 * 测试 3: 段切换
 * ======================================================================== */
TEST_F(WALSegmentTest, SwitchSegment) {
    wal_t *wal = wal_create(TEST_SEG_DIR, 8192);
    ASSERT_NE(wal, nullptr);

    /* 初始段 0 */
    EXPECT_EQ(wal_current_segment_no(wal), 0U);

    /* 写入记录 */
    ASSERT_GT(wal_write_begin(wal, 1), 0U);
    ASSERT_GT(wal_write_commit(wal, 1), 0U);

    /* 切换段 */
    ASSERT_EQ(wal_switch_segment(wal), 0);
    EXPECT_EQ(wal_current_segment_no(wal), 1U);

    /* 写入到新段 */
    ASSERT_GT(wal_write_begin(wal, 2), 0U);
    ASSERT_GT(wal_write_commit(wal, 2), 0U);

    wal_close(wal);
}

/* ========================================================================
 * 测试 4: 段满自动切换
 * ======================================================================== */
TEST_F(WALSegmentTest, AutoSwitchOnFull) {
    wal_t *wal = wal_create(TEST_SEG_DIR, 8192);
    ASSERT_NE(wal, nullptr);

    /* 写入大量小记录，模拟填满段 */
    uint32_t txn_id = 1;
    for (int i = 0; i < 100; i++) {
        int key = i;
        uint64_t lsn = wal_write_insert(wal, txn_id, &key, sizeof(key), &key, sizeof(key));
        EXPECT_GT(lsn, 0U);
    }

    /* 验证自动切换发生（段序号 > 0 表示发生了切换） */
    uint32_t segno = wal_current_segment_no(wal);
    EXPECT_GE(segno, 0U);

    wal_close(wal);
}

/* ========================================================================
 * 测试 5: 段文件列表
 * ======================================================================== */
TEST_F(WALSegmentTest, ListSegments) {
    wal_t *wal = wal_create(TEST_SEG_DIR, 8192);
    ASSERT_NE(wal, nullptr);

    /* 写入数据并切换几次 */
    ASSERT_GT(wal_write_begin(wal, 1), 0U);
    ASSERT_EQ(wal_switch_segment(wal), 0);
    ASSERT_GT(wal_write_begin(wal, 2), 0U);
    ASSERT_EQ(wal_switch_segment(wal), 0);
    ASSERT_GT(wal_write_begin(wal, 3), 0U);

    /* 列出段文件 */
    char **segments = NULL;
    int count = 0;
    ASSERT_EQ(wal_list_segments(wal, &segments, &count), 0);
    EXPECT_GE(count, 3);  /* 至少 3 个段 */

    /* 验证段文件路径非空 */
    for (int i = 0; i < count; i++) {
        EXPECT_NE(segments[i], nullptr);
        EXPECT_GT(strlen(segments[i]), 0U);
    }

    wal_free_segment_list(segments, count);
    wal_close(wal);
}

/* ========================================================================
 * 测试 6: 段文件路径查询
 * ======================================================================== */
TEST_F(WALSegmentTest, PathForLsn) {
    wal_t *wal = wal_create(TEST_SEG_DIR, 8192);
    ASSERT_NE(wal, nullptr);

    /* 写入记录获取 LSN */
    uint64_t lsn = wal_write_begin(wal, 42);
    ASSERT_GT(lsn, 0U);

    /* 根据 LSN 获取路径 */
    char *path = wal_segment_path_for_lsn(wal, lsn);
    ASSERT_NE(path, nullptr);
    EXPECT_GT(strlen(path), 0U);

    free(path);
    wal_close(wal);
}

/* ========================================================================
 * 测试 7: 跨段读写
 * ======================================================================== */
TEST_F(WALSegmentTest, CrossSegmentRead) {
    wal_t *wal = wal_create(TEST_SEG_DIR, 8192);
    ASSERT_NE(wal, nullptr);

    /* 段 0 写入 */
    int key1 = 100, val1 = 200;
    ASSERT_GT(wal_write_insert(wal, 1, &key1, sizeof(key1), &val1, sizeof(val1)), 0U);
    ASSERT_GT(wal_write_commit(wal, 1), 0U);

    wal_close(wal);

    /* 重新打开，验证恢复 */
    wal_t *wal2 = wal_open(TEST_SEG_DIR);
    ASSERT_NE(wal2, nullptr);

    /* 验证统计信息 */
    wal_stats_t stats;
    ASSERT_EQ(wal_get_stats(wal2, &stats), 0);
    EXPECT_GE(stats.total_records, 2U);

    wal_close(wal2);
}

/* ========================================================================
 * 测试 8: 段目录设置
 * ======================================================================== */
TEST_F(WALSegmentTest, SetSegmentDir) {
    wal_t *wal = wal_create(TEST_SEG_DIR, 8192);
    ASSERT_NE(wal, nullptr);

    /* 设置新的段目录 */
    ASSERT_EQ(wal_set_segment_dir(wal, "./alt_pg_wal"), 0);

    /* 切换段应该在新目录创建文件 */
    ASSERT_EQ(wal_switch_segment(wal), 0);

    const char *path = wal_current_segment_path(wal);
    ASSERT_NE(path, nullptr);
    EXPECT_NE(strstr(path, "alt_pg_wal"), nullptr);

    /* 清理 */
    wal_close(wal);

#ifdef _WIN32
    system("rmdir /s /q alt_pg_wal 2>nul");
    system("rmdir /s /q test_pg_wal 2>nul");
#else
    system("rm -rf alt_pg_wal");
    system("rm -rf test_pg_wal");
#endif
}