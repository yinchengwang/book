/**
 * @file test_stat_views.cpp
 * @brief 统计视图测试
 */
#include <gtest/gtest.h>
#include <db/tools/stats.h>
#include <cstring>
#include <cstdlib>

class StatViewsTest : public ::testing::Test {
protected:
    void SetUp() override {
        sc = stats_init();
        ASSERT_NE(sc, nullptr);
    }

    void TearDown() override {
        stats_shutdown(sc);
        if (result) free(result);
    }

    StatsCollector *sc;
    char *result = nullptr;
};

/* 数据库视图 - pipe-separated 格式 */
TEST_F(StatViewsTest, DatabaseView)
{
    int ret = stat_view_database(sc, &result);
    EXPECT_EQ(0, ret);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(strstr(result, "datname") != nullptr);
    EXPECT_TRUE(strstr(result, "default_db") != nullptr);
}

/* 数据库视图 - JSON 格式 */
TEST_F(StatViewsTest, DatabaseViewJson)
{
    int ret = stat_view_database_json(sc, &result);
    EXPECT_EQ(0, ret);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(strstr(result, "\"datname\"") != nullptr);
    EXPECT_TRUE(strstr(result, "\"default_db\"") != nullptr);
}

/* 数据库视图 - 空指针参数检查 */
TEST_F(StatViewsTest, DatabaseViewWithNull)
{
    int ret = stat_view_database(sc, NULL);
    EXPECT_NE(0, ret);
}

/* 表视图 - 空表 pipe-separated 格式 */
TEST_F(StatViewsTest, TablesViewEmpty)
{
    int ret = stat_view_tables(sc, &result);
    EXPECT_EQ(0, ret);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(strstr(result, "relname") != nullptr);
}

/* 表视图 - 空表 JSON 格式 */
TEST_F(StatViewsTest, TablesViewJsonEmpty)
{
    int ret = stat_view_tables_json(sc, &result);
    EXPECT_EQ(0, ret);
    ASSERT_NE(result, nullptr);
    /* 空表时输出包含 "[]" 或 JSON 数组结构 */
    EXPECT_TRUE(strstr(result, "[") != nullptr && strstr(result, "]") != nullptr);
}

/* 表视图 - 空指针参数检查 */
TEST_F(StatViewsTest, TablesViewWithNull)
{
    int ret = stat_view_tables(sc, NULL);
    EXPECT_NE(0, ret);
}

/* 数据库视图 - 空收集器检查 */
TEST_F(StatViewsTest, DatabaseViewNullCollector)
{
    char *r = nullptr;
    EXPECT_NE(0, stat_view_database(NULL, &r));
    EXPECT_NE(0, stat_view_database_json(NULL, &r));
}

/* 表视图 - 空收集器检查 */
TEST_F(StatViewsTest, TablesViewNullCollector)
{
    char *r = nullptr;
    EXPECT_NE(0, stat_view_tables(NULL, &r));
    EXPECT_NE(0, stat_view_tables_json(NULL, &r));
}

/* 数据库视图 - 命中率计算 */
TEST_F(StatViewsTest, DatabaseViewHitRatio)
{
    /* 初始状态 blks_read 和 blks_hit 均为 0，命中率应为 0.0000 */
    int ret = stat_view_database(sc, &result);
    EXPECT_EQ(0, ret);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(strstr(result, "0.0000") != nullptr);
}

/* 数据库视图 JSON - 包含所有统计字段 */
TEST_F(StatViewsTest, DatabaseViewJsonFields)
{
    int ret = stat_view_database_json(sc, &result);
    EXPECT_EQ(0, ret);
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(strstr(result, "\"xact_commit\"") != nullptr);
    EXPECT_TRUE(strstr(result, "\"xact_rollback\"") != nullptr);
    EXPECT_TRUE(strstr(result, "\"tup_inserted\"") != nullptr);
    EXPECT_TRUE(strstr(result, "\"tup_updated\"") != nullptr);
    EXPECT_TRUE(strstr(result, "\"tup_deleted\"") != nullptr);
    EXPECT_TRUE(strstr(result, "\"hit_ratio\"") != nullptr);
}
