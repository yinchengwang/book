/**
 * @file test_reindex.cpp
 * @brief REINDEX 索引重建工具测试
 *
 * 测试 REINDEX 各范围级别的索引重建功能
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/tools/reindex.h"
}

/* ========================================================================
 * ReindexOptions 测试
 * ======================================================================== */

class ReindexTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ReindexTest, DefaultOptions)
{
    ReindexOptions opts = reindex_default_options();
    EXPECT_FALSE(opts.verbose);
    EXPECT_FALSE(opts.concurrently);
    EXPECT_EQ(0, opts.parallel_workers);
}

TEST_F(ReindexTest, ReindexIndex)
{
    ReindexOptions opts = reindex_default_options();
    ReindexStats stats;

    int ret = reindex_index("idx_test", &opts, &stats);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, stats.num_indexes);
}

TEST_F(ReindexTest, ReindexIndexNullName)
{
    ReindexOptions opts = reindex_default_options();
    ReindexStats stats;

    int ret = reindex_index(NULL, &opts, &stats);
    EXPECT_NE(0, ret);
}

TEST_F(ReindexTest, ReindexTable)
{
    ReindexOptions opts = reindex_default_options();
    ReindexStats stats;

    int ret = reindex_table("test_table", &opts, &stats);
    EXPECT_EQ(0, ret);
}

TEST_F(ReindexTest, ReindexTableNullName)
{
    ReindexOptions opts = reindex_default_options();
    ReindexStats stats;

    int ret = reindex_table(NULL, &opts, &stats);
    EXPECT_NE(0, ret);
}

TEST_F(ReindexTest, ReindexDatabase)
{
    ReindexOptions opts = reindex_default_options();
    ReindexStats stats;

    int ret = reindex_database(NULL, &opts, &stats);
    EXPECT_EQ(0, ret);
}

TEST_F(ReindexTest, ReindexDatabaseNullStats)
{
    ReindexOptions opts = reindex_default_options();

    int ret = reindex_database(NULL, &opts, NULL);
    EXPECT_NE(0, ret);
}

TEST_F(ReindexTest, ReindexSystem)
{
    ReindexOptions opts = reindex_default_options();
    ReindexStats stats;

    int ret = reindex_system(&opts, &stats);
    EXPECT_EQ(0, ret);
}

TEST_F(ReindexTest, ReindexSystemNullStats)
{
    ReindexOptions opts = reindex_default_options();

    int ret = reindex_system(&opts, NULL);
    EXPECT_NE(0, ret);
}
