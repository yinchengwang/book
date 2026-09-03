/*
 * 重建策略测试
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/delete/rebuild_strategy.h>

class RebuildStrategyTest : public ::testing::Test {
protected:
    void SetUp() override {
        strategy_ = rebuild_strategy_create(nullptr);
        ASSERT_NE(strategy_, nullptr);
    }

    void TearDown() override {
        if (strategy_) {
            rebuild_strategy_destroy(strategy_);
            strategy_ = nullptr;
        }
    }

    rebuild_strategy_t *strategy_;
};

TEST_F(RebuildStrategyTest, CreateAndDestroy)
{
    EXPECT_NE(strategy_, nullptr);
}

TEST_F(RebuildStrategyTest, NoDeleted)
{
    /* 没有删除时，无需重建 */
    rebuild_decision_t decision = rebuild_strategy_decide(strategy_, 0, 1000);
    EXPECT_EQ(decision, REBUILD_DECISION_NONE);
}

TEST_F(RebuildStrategyTest, LowDeletedRatio)
{
    /* 低删除比例，使用边修复 */
    rebuild_decision_t decision = rebuild_strategy_decide(strategy_, 100, 10000);
    EXPECT_EQ(decision, REBUILD_DECISION_REPAIR);
}

TEST_F(RebuildStrategyTest, HighDeletedRatio)
{
    /* 高删除比例，使用重建 */
    rebuild_decision_t decision = rebuild_strategy_decide(strategy_, 5000, 10000);
    EXPECT_EQ(decision, REBUILD_DECISION_REBUILD);
}

TEST_F(RebuildStrategyTest, DeletedRatioCalculation)
{
    EXPECT_FLOAT_EQ(rebuild_strategy_deleted_ratio(0, 1000), 0.0f);
    EXPECT_FLOAT_EQ(rebuild_strategy_deleted_ratio(100, 1000), 0.1f);
    EXPECT_FLOAT_EQ(rebuild_strategy_deleted_ratio(500, 1000), 0.5f);
    EXPECT_FLOAT_EQ(rebuild_strategy_deleted_ratio(1000, 1000), 1.0f);
}

TEST_F(RebuildStrategyTest, ZeroTotal)
{
    /* 总数为 0 时 */
    rebuild_decision_t decision = rebuild_strategy_decide(strategy_, 100, 0);
    EXPECT_EQ(decision, REBUILD_DECISION_NONE);
}

TEST_F(RebuildStrategyTest, CustomConfig)
{
    rebuild_strategy_config_t config;
    rebuild_strategy_config_init_default(&config);
    config.deleted_ratio_threshold = 50;  /* 50% 才重建 */
    config.prefer_rebuild = true;

    rebuild_strategy_update_config(strategy_, &config);

    /* 40% 删除应该选择重建（因为 prefer_rebuild=true） */
    rebuild_decision_t decision = rebuild_strategy_decide(strategy_, 400, 1000);
    EXPECT_EQ(decision, REBUILD_DECISION_REBUILD);
}

TEST_F(RebuildStrategyTest, NullStrategy)
{
    /* 空策略使用默认行为 */
    rebuild_decision_t decision = rebuild_strategy_decide(nullptr, 100, 1000);
    EXPECT_EQ(decision, REBUILD_DECISION_REPAIR);
}
