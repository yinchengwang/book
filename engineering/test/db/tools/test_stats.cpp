/**
 * @file test_stats.cpp
 * @brief 统计收集器测试
 */
#include <gtest/gtest.h>
#include <db/tools/stats.h>
#include <cstring>

class StatsTest : public ::testing::Test {
protected:
    void SetUp() override {
        sc = stats_init();
        ASSERT_NE(sc, nullptr);
    }

    void TearDown() override {
        stats_shutdown(sc);
    }

    StatsCollector *sc;
};

/* 初始化与关闭 */
TEST_F(StatsTest, InitShutdown)
{
    StatsCollector *sc2 = stats_init();
    ASSERT_NE(sc2, nullptr);
    stats_shutdown(sc2);
}

/* 获取数据库级统计 */
TEST_F(StatsTest, GetDatabase)
{
    StatDatabase db;
    int ret = stats_get_database(sc, &db);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, db.xact_commit);
    EXPECT_EQ(0, db.tup_inserted);
}

/* 空指针参数检查 */
TEST_F(StatsTest, GetDatabaseWithNull)
{
    int ret = stats_get_database(sc, NULL);
    EXPECT_NE(0, ret);
}

/* 空表级统计 */
TEST_F(StatsTest, GetTablesEmpty)
{
    StatTable tables[10];
    int count = 0;
    int ret = stats_get_tables(sc, NULL, tables, 10, &count);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, count);
}

/* 空索引级统计 */
TEST_F(StatsTest, GetIndexesEmpty)
{
    StatIndex indexes[10];
    int count = 0;
    int ret = stats_get_indexes(sc, NULL, indexes, 10, &count);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, count);
}

/* 重置统计信息 */
TEST_F(StatsTest, Reset)
{
    StatDatabase db;
    stats_reset(sc);
    int ret = stats_get_database(sc, &db);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, db.xact_commit);
}

/* 空指针收集器检查 */
TEST_F(StatsTest, NullCollector)
{
    StatDatabase db;
    EXPECT_NE(0, stats_get_database(NULL, &db));

    StatTable tables[10];
    int count = 0;
    EXPECT_NE(0, stats_get_tables(NULL, NULL, tables, 10, &count));

    StatIndex indexes[10];
    EXPECT_NE(0, stats_get_indexes(NULL, NULL, indexes, 10, &count));
}

/* 重置空收集器不崩溃 */
TEST_F(StatsTest, ResetNull)
{
    stats_reset(NULL);  /* 不应崩溃 */
}
