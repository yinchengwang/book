/**
 * @file test_vacuum.cpp
 * @brief VACUUM 工具测试
 *
 * 测试 VACUUM、VACUUM FULL、ANALYZE 功能
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/tools/vacuum.h"
}

/* ========================================================================
 * VacuumOptions 测试
 * ======================================================================== */

class VacuumTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(VacuumTest, DefaultOptions)
{
    VacuumOptions opts = vacuum_default_options();
    EXPECT_FALSE(opts.full);
    EXPECT_FALSE(opts.analyze);
    EXPECT_FALSE(opts.verbose);
    EXPECT_FALSE(opts.freeze);
    EXPECT_FALSE(opts.disable_page_skipping);
    EXPECT_EQ(0, opts.index_cleanup);
    EXPECT_EQ(0, opts.truncate);
    EXPECT_EQ(0, opts.parallel_workers);
}

TEST_F(VacuumTest, CreateDestroy)
{
    VacuumOptions opts = vacuum_default_options();
    VacuumContext *ctx = vacuum_create(&opts);
    ASSERT_NE(nullptr, ctx);
    vacuum_destroy(ctx);
}

TEST_F(VacuumTest, CreateWithNullOptions)
{
    VacuumContext *ctx = vacuum_create(NULL);
    ASSERT_NE(nullptr, ctx);
    vacuum_destroy(ctx);
}

TEST_F(VacuumTest, ExecuteWithNullTable)
{
    VacuumOptions opts = vacuum_default_options();
    VacuumContext *ctx = vacuum_create(&opts);
    ASSERT_NE(nullptr, ctx);

    VacuumStats stats;
    int ret = vacuum_execute(ctx, NULL, &stats);
    EXPECT_EQ(0, ret);
    EXPECT_GE(stats.execution_time_ms, 0.0);

    vacuum_destroy(ctx);
}

TEST_F(VacuumTest, ExecuteWithTableName)
{
    VacuumOptions opts = vacuum_default_options();
    VacuumContext *ctx = vacuum_create(&opts);
    ASSERT_NE(nullptr, ctx);

    VacuumStats stats;
    int ret = vacuum_execute(ctx, "test_table", &stats);
    EXPECT_EQ(0, ret);
    EXPECT_GE(stats.execution_time_ms, 0.0);

    vacuum_destroy(ctx);
}

TEST_F(VacuumTest, ExecuteWithNullStats)
{
    VacuumOptions opts = vacuum_default_options();
    VacuumContext *ctx = vacuum_create(&opts);
    ASSERT_NE(nullptr, ctx);

    int ret = vacuum_execute(ctx, "test_table", NULL);
    EXPECT_NE(0, ret);  /* 应该失败 */

    vacuum_destroy(ctx);
}

TEST_F(VacuumTest, FullExecuteWithNullTable)
{
    VacuumOptions opts = vacuum_default_options();
    opts.full = true;
    VacuumContext *ctx = vacuum_create(&opts);
    ASSERT_NE(nullptr, ctx);

    VacuumStats stats;
    int ret = vacuum_full_execute(ctx, NULL, &stats);
    EXPECT_NE(0, ret);  /* 空表名应该失败 */

    vacuum_destroy(ctx);
}

TEST_F(VacuumTest, FullExecuteWithTableName)
{
    VacuumOptions opts = vacuum_default_options();
    opts.full = true;
    VacuumContext *ctx = vacuum_create(&opts);
    ASSERT_NE(nullptr, ctx);

    VacuumStats stats;
    int ret = vacuum_full_execute(ctx, "test_table", &stats);
    EXPECT_EQ(0, ret);
    EXPECT_GE(stats.execution_time_ms, 0.0);

    vacuum_destroy(ctx);
}

TEST_F(VacuumTest, FullExecuteWithAnalyze)
{
    VacuumOptions opts = vacuum_default_options();
    opts.full = true;
    opts.analyze = true;
    VacuumContext *ctx = vacuum_create(&opts);
    ASSERT_NE(nullptr, ctx);

    VacuumStats stats;
    int ret = vacuum_full_execute(ctx, "test_table", &stats);
    EXPECT_EQ(0, ret);

    vacuum_destroy(ctx);
}

TEST_F(VacuumTest, GetError)
{
    VacuumOptions opts = vacuum_default_options();
    VacuumContext *ctx = vacuum_create(&opts);
    ASSERT_NE(nullptr, ctx);

    const char *err = vacuum_get_error(ctx);
    EXPECT_NE(nullptr, err);

    vacuum_destroy(ctx);
}

TEST_F(VacuumTest, GetErrorWithNullContext)
{
    const char *err = vacuum_get_error(NULL);
    EXPECT_NE(nullptr, err);
    EXPECT_STREQ("上下文为 NULL", err);
}

TEST_F(VacuumTest, FullExecuteWithNullStats)
{
    VacuumOptions opts = vacuum_default_options();
    opts.full = true;
    VacuumContext *ctx = vacuum_create(&opts);
    ASSERT_NE(nullptr, ctx);

    int ret = vacuum_full_execute(ctx, "test_table", NULL);
    EXPECT_NE(0, ret);  /* 应该失败 */

    vacuum_destroy(ctx);
}

/* ========================================================================
 * VacuumStats 测试
 * ======================================================================== */

TEST_F(VacuumTest, StatsInitialization)
{
    VacuumOptions opts = vacuum_default_options();
    VacuumContext *ctx = vacuum_create(&opts);
    ASSERT_NE(nullptr, ctx);

    VacuumStats stats;
    int ret = vacuum_execute(ctx, "test_table", &stats);
    EXPECT_EQ(0, ret);

    /* 骨架实现应该将所有统计初始化为 0 */
    EXPECT_EQ(0, stats.num_pages);
    EXPECT_EQ(0, stats.num_tuples);
    EXPECT_EQ(0, stats.num_dead_tuples);
    EXPECT_EQ(0, stats.pages_removed);
    EXPECT_EQ(0, stats.tuples_removed);
    EXPECT_EQ(0, stats.pages_frozen);
    EXPECT_EQ(0, stats.index_pages_removed);
    EXPECT_GE(stats.execution_time_ms, 0.0);

    vacuum_destroy(ctx);
}

/* ========================================================================
 * AnalyzeOptions 测试
 * ======================================================================== */

class AnalyzeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(AnalyzeTest, DefaultOptions)
{
    AnalyzeOptions opts = analyze_default_options();
    EXPECT_FALSE(opts.verbose);
}

TEST_F(AnalyzeTest, ExecuteWithNullTable)
{
    AnalyzeStats stats;
    int ret = analyze_execute(NULL, NULL, 0, &stats);
    EXPECT_EQ(0, ret);
    EXPECT_GE(stats.execution_time_ms, 0.0);
}

TEST_F(AnalyzeTest, ExecuteWithTableName)
{
    AnalyzeStats stats;
    int ret = analyze_execute("test_table", NULL, 0, &stats);
    EXPECT_EQ(0, ret);
    EXPECT_GE(stats.execution_time_ms, 0.0);
}

TEST_F(AnalyzeTest, ExecuteWithColumns)
{
    const char *columns[] = {"col1", "col2"};
    AnalyzeStats stats;
    int ret = analyze_execute("test_table", columns, 2, &stats);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(2, stats.num_columns);
}

TEST_F(AnalyzeTest, ExecuteWithNullStats)
{
    int ret = analyze_execute("test_table", NULL, 0, NULL);
    EXPECT_NE(0, ret);  /* 应该失败 */
}

TEST_F(AnalyzeTest, StatsInitialization)
{
    AnalyzeStats stats;
    int ret = analyze_execute("test_table", NULL, 0, &stats);
    EXPECT_EQ(0, ret);

    /* 骨架实现应该将统计初始化为 0 */
    EXPECT_EQ(0, stats.num_pages);
    EXPECT_EQ(0, stats.num_tuples);
    EXPECT_EQ(0, stats.num_columns);
    EXPECT_GE(stats.execution_time_ms, 0.0);
}
