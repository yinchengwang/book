/**
 * @file buf_test.cpp
 * @brief Buffer Pool 单元测试
 */

#include "gtest/gtest.h"
#include "db/buf.h"

/* ============================================================
 * 测试夹具
 * ============================================================ */

class BufferPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(0, buf_init(64));  /* 小缓冲池便于测试 */
    }

    void TearDown() override {
        buf_shutdown();
    }
};

/* ============================================================
 * 基础测试
 * ============================================================ */

TEST_F(BufferPoolTest, InitAndShutdown) {
    /* 重复初始化应该成功 */
    EXPECT_EQ(0, buf_init(32));

    /* 检查 Buffer 数量 */
    EXPECT_EQ(64, buf_get_nbuffers());
}

TEST_F(BufferPoolTest, GetNbuffers) {
    EXPECT_EQ(64, buf_get_nbuffers());
}

/* ============================================================
 * 页面读取测试
 * ============================================================ */

TEST_F(BufferPoolTest, ReadPage) {
    /* 读取一个页面 */
    BufferDesc *buf = buf_read(1, 0, 0);
    ASSERT_NE(nullptr, buf);

    EXPECT_EQ(1u, buf_get_relfilenode(buf));
    EXPECT_EQ(0u, buf_get_blocknum(buf));
    EXPECT_TRUE(buf_isvalid(buf));
    EXPECT_FALSE(buf_isdirty(buf));

    buf_unpin(buf);
}

TEST_F(BufferPoolTest, ReadPageWithWriteAccess) {
    /* 读取一个页面并请求写权限 */
    BufferDesc *buf = buf_read(1, 0, 1);
    ASSERT_NE(nullptr, buf);

    EXPECT_TRUE(buf_isdirty(buf));

    buf_unpin(buf);
}

TEST_F(BufferPoolTest, ReadSamePageTwice) {
    /* 多次读取同一页面应该命中 */
    BufferDesc *buf1 = buf_read(1, 0, 0);
    ASSERT_NE(nullptr, buf1);
    buf_unpin(buf1);

    BufferDesc *buf2 = buf_read(1, 0, 0);
    ASSERT_NE(nullptr, buf2);

    /* 应该是同一个 Buffer */
    EXPECT_EQ(buf_get_id(buf1), buf_get_id(buf2));

    buf_unpin(buf2);
}

TEST_F(BufferPoolTest, ReadDifferentPages) {
    /* 读取不同页面 */
    BufferDesc *buf1 = buf_read(1, 0, 0);
    BufferDesc *buf2 = buf_read(1, 1, 0);
    BufferDesc *buf3 = buf_read(2, 0, 0);

    ASSERT_NE(nullptr, buf1);
    ASSERT_NE(nullptr, buf2);
    ASSERT_NE(nullptr, buf3);

    EXPECT_EQ(0u, buf_get_blocknum(buf1));
    EXPECT_EQ(1u, buf_get_blocknum(buf2));
    EXPECT_EQ(0u, buf_get_blocknum(buf3));

    /* 不同关系 */
    EXPECT_EQ(1u, buf_get_relfilenode(buf1));
    EXPECT_EQ(1u, buf_get_relfilenode(buf2));
    EXPECT_EQ(2u, buf_get_relfilenode(buf3));

    buf_unpin(buf1);
    buf_unpin(buf2);
    buf_unpin(buf3);
}

/* ============================================================
 * Pin/Unpin 测试
 * ============================================================ */

TEST_F(BufferPoolTest, PinUnpin) {
    BufferDesc *buf = buf_read(1, 0, 0);
    ASSERT_NE(nullptr, buf);

    /* 初始状态 */
    EXPECT_TRUE(buf_ispinned(buf));

    /* 多次 pin */
    buf_pin(buf);
    buf_pin(buf);
    EXPECT_TRUE(buf_ispinned(buf));

    /* 一次 unpin */
    buf_unpin(buf);
    EXPECT_TRUE(buf_ispinned(buf));

    /* 完全 unpin */
    buf_unpin(buf);
    buf_unpin(buf);
    EXPECT_FALSE(buf_ispinned(buf));
}

TEST_F(BufferPoolTest, PinIncreasesUsageCount) {
    BufferDesc *buf = buf_read(1, 0, 0);
    ASSERT_NE(nullptr, buf);

    int initial_usage = 1;  /* 读取时设置的初始值 */

    buf_unpin(buf);
    buf_pin(buf);

    EXPECT_GE(buf->usage_count, initial_usage);
}

/* ============================================================
 * 脏页标记测试
 * ============================================================ */

TEST_F(BufferPoolTest, DirtyFlag) {
    BufferDesc *buf = buf_read(1, 0, 0);
    ASSERT_NE(nullptr, buf);

    EXPECT_FALSE(buf_isdirty(buf));

    buf_dirty(buf);
    EXPECT_TRUE(buf_isdirty(buf));

    buf_clean(buf);
    EXPECT_FALSE(buf_isdirty(buf));

    buf_unpin(buf);
}

TEST_F(BufferPoolTest, WriteAccessMarksDirty) {
    BufferDesc *buf = buf_read(1, 0, 1);  /* access_mode = 1 表示写访问 */
    ASSERT_NE(nullptr, buf);

    EXPECT_TRUE(buf_isdirty(buf));

    buf_unpin(buf);
}

/* ============================================================
 * 新页面分配测试
 * ============================================================ */

TEST_F(BufferPoolTest, NewPage) {
    BufferDesc *buf = buf_new(1);
    ASSERT_NE(nullptr, buf);

    EXPECT_TRUE(buf_isvalid(buf));
    EXPECT_TRUE(buf_isdirty(buf));  /* 新页面应该标记为脏 */

    buf_unpin(buf);
}

TEST_F(BufferPoolTest, NewTempPage) {
    BufferDesc *buf = buf_new_temp();
    ASSERT_NE(nullptr, buf);

    EXPECT_TRUE(buf_isvalid(buf));
    EXPECT_FALSE(buf_isdirty(buf));  /* 临时页面不脏 */

    buf_unpin(buf);
}

/* ============================================================
 * 统计信息测试
 * ============================================================ */

TEST_F(BufferPoolTest, CacheHitStats) {
    /* 重置统计 */
    buf_reset_stats();

    /* 第一次读取 - 未命中 */
    BufferDesc *buf1 = buf_read(1, 0, 0);
    ASSERT_NE(nullptr, buf1);
    buf_unpin(buf1);

    /* 第二次读取 - 命中 */
    BufferDesc *buf2 = buf_read(1, 0, 0);
    ASSERT_NE(nullptr, buf2);
    buf_unpin(buf2);

    /* 第三次读取 - 命中 */
    BufferDesc *buf3 = buf_read(1, 0, 0);
    ASSERT_NE(nullptr, buf3);
    buf_unpin(buf3);

    buf_stats_t stats;
    buf_get_stats(&stats);

    EXPECT_EQ(1u, stats.misses);
    EXPECT_GE(stats.hits, 2u);
}

TEST_F(BufferPoolTest, WriteStats) {
    buf_reset_stats();

    BufferDesc *buf = buf_new(1);
    ASSERT_NE(nullptr, buf);
    buf_write(buf);
    buf_unpin(buf);

    buf_stats_t stats;
    buf_get_stats(&stats);

    EXPECT_GE(stats.writes, 1u);
}

/* ============================================================
 * 刷盘测试
 * ============================================================ */

TEST_F(BufferPoolTest, FlushAll) {
    /* 创建多个脏页 */
    buf_new(1);
    buf_new(1);
    buf_new(2);

    int flushed = buf_flush_all();
    EXPECT_GE(flushed, 1);
}

TEST_F(BufferPoolTest, FlushRelation) {
    /* 创建指定关系的脏页 */
    buf_new(1);
    buf_new(1);
    buf_new(2);  /* 不同关系 */

    int flushed = buf_flush_relation(1);
    EXPECT_EQ(2, flushed);

    int flushed2 = buf_flush_relation(2);
    EXPECT_EQ(1, flushed2);
}

/* ============================================================
 * 数据访问测试
 * ============================================================ */

TEST_F(BufferPoolTest, DataAccess) {
    BufferDesc *buf = buf_read(1, 0, 1);
    ASSERT_NE(nullptr, buf);

    /* 获取数据指针并写入 */
    char *data = (char *)buf_get_data(buf);
    ASSERT_NE(nullptr, data);

    /* 写入一些数据 */
    strcpy(data, "Hello, Buffer Pool!");
    data[19] = '\0';

    buf_unpin(buf);

    /* 再次读取，应该能看到数据 */
    BufferDesc *buf2 = buf_read(1, 0, 0);
    ASSERT_NE(nullptr, buf2);

    char *data2 = (char *)buf_get_data(buf2);
    EXPECT_STREQ("Hello, Buffer Pool!", data2);

    buf_unpin(buf2);
}

/* ============================================================
 * 状态查询测试
 * ============================================================ */

TEST_F(BufferPoolTest, StateQueries) {
    BufferDesc *buf = buf_read(1, 0, 0);
    ASSERT_NE(nullptr, buf);

    /* 检查各种状态 */
    EXPECT_TRUE(buf_isvalid(buf));
    EXPECT_EQ(0u, buf_get_blocknum(buf));
    EXPECT_EQ(1u, buf_get_relfilenode(buf));
    EXPECT_GE(buf_get_id(buf), 0);

    BufferState state = buf_get_state(buf);
    EXPECT_NE(0u, state);

    buf_unpin(buf);
}

/* ============================================================
 * 置换测试（压力测试）
 * ============================================================ */

TEST_F(BufferPoolTest, BufferReplacement) {
    /* 在小缓冲池中读取大量页面，触发置换 */
    const int npages = 200;  /* 远大于 64 个 buffer */

    for (int i = 0; i < npages; i++) {
        BufferDesc *buf = buf_read(1, (BlockNumber)i, 0);
        ASSERT_NE(nullptr, buf);
        buf_unpin(buf);
    }

    /* 验证仍然可以访问任意页面 */
    BufferDesc *buf = buf_read(1, 100, 0);
    ASSERT_NE(nullptr, buf);
    buf_unpin(buf);

    buf_stats_t stats;
    buf_get_stats(&stats);
    EXPECT_GE(stats.misses, (uint64_t)npages);
}
