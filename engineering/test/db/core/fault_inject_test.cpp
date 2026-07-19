/**
 * @file fault_inject_test.cpp
 * @brief 故障注入框架测试
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/fault_inject.h"
}

/**
 * @brief 故障注入测试夹具
 */
class FaultInjectTest : public ::testing::Test {
protected:
    void SetUp() override {
        fault_init();
    }

    void TearDown() override {
        fault_clear();
    }
};

/**
 * @brief 初始化和清除
 */
TEST_F(FaultInjectTest, InitAndClear) {
    fault_stats_t stats;
    fault_get_stats(&stats);
    EXPECT_EQ(stats.active_rules, 0u);
    EXPECT_EQ(stats.total_triggers, 0u);

    /* 清除后仍处于有效状态 */
    fault_clear();
    fault_get_stats(&stats);
    EXPECT_EQ(stats.active_rules, 0u);
}

/**
 * @brief 注册和触发
 */
TEST_F(FaultInjectTest, RegisterAndTrigger) {
    fault_config_t cfg = {
        .type = FAULT_DISK_READ,
        .point = FAULT_POINT_READ_PAGE,
        .skip_count = 0,
        .max_hits = 3,
        .hits = 0,
    };
    EXPECT_EQ(fault_register(&cfg), 0);

    /* 前 3 次触发 */
    EXPECT_TRUE(fault_should_fail(FAULT_POINT_READ_PAGE));
    EXPECT_TRUE(fault_should_fail(FAULT_POINT_READ_PAGE));
    EXPECT_TRUE(fault_should_fail(FAULT_POINT_READ_PAGE));

    /* 第 4 次不应触发（max_hits=3） */
    EXPECT_FALSE(fault_should_fail(FAULT_POINT_READ_PAGE));

    /* 验证统计 */
    fault_stats_t stats;
    fault_get_stats(&stats);
    EXPECT_EQ(stats.total_triggers, 3u);
}

/**
 * @brief 跳过计数
 */
TEST_F(FaultInjectTest, SkipCount) {
    fault_config_t cfg = {
        .type = FAULT_DISK_WRITE,
        .point = FAULT_POINT_WRITE_PAGE,
        .skip_count = 5,
        .max_hits = 1,
        .hits = 0,
    };
    EXPECT_EQ(fault_register(&cfg), 0);

    /* 前 5 次不应触发 */
    for (int i = 0; i < 5; i++) {
        EXPECT_FALSE(fault_should_fail(FAULT_POINT_WRITE_PAGE)) << "at iteration " << i;
    }

    /* 第 6 次触发 */
    EXPECT_TRUE(fault_should_fail(FAULT_POINT_WRITE_PAGE));

    /* 之后不再触发 */
    EXPECT_FALSE(fault_should_fail(FAULT_POINT_WRITE_PAGE));
}

/**
 * @brief 多条规则
 */
TEST_F(FaultInjectTest, MultipleRules) {
    fault_config_t read_cfg = {
        .type = FAULT_DISK_READ,
        .point = FAULT_POINT_READ_PAGE,
        .skip_count = 0,
        .max_hits = 0,  /* 无限 */
        .hits = 0,
    };
    fault_config_t write_cfg = {
        .type = FAULT_DISK_WRITE,
        .point = FAULT_POINT_WRITE_PAGE,
        .skip_count = 2,
        .max_hits = 1,
        .hits = 0,
    };
    EXPECT_EQ(fault_register(&read_cfg), 0);
    EXPECT_EQ(fault_register(&write_cfg), 0);

    /* 读故障始终触发 */
    EXPECT_TRUE(fault_should_fail(FAULT_POINT_READ_PAGE));
    EXPECT_TRUE(fault_should_fail(FAULT_POINT_READ_PAGE));

    /* 写故障需要跳过 2 次 */
    EXPECT_FALSE(fault_should_fail(FAULT_POINT_WRITE_PAGE));
    EXPECT_FALSE(fault_should_fail(FAULT_POINT_WRITE_PAGE));
    EXPECT_TRUE(fault_should_fail(FAULT_POINT_WRITE_PAGE));

    /* 不同注入点互不影响 */
    EXPECT_TRUE(fault_should_fail(FAULT_POINT_READ_PAGE));
    EXPECT_FALSE(fault_should_fail(FAULT_POINT_WRITE_PAGE));
}

/**
 * @brief 清除后恢复
 */
TEST_F(FaultInjectTest, ClearRecovery) {
    fault_config_t cfg = {
        .type = FAULT_DISK_READ,
        .point = FAULT_POINT_READ_PAGE,
        .skip_count = 0,
        .max_hits = 0,
        .hits = 0,
    };
    EXPECT_EQ(fault_register(&cfg), 0);
    EXPECT_TRUE(fault_should_fail(FAULT_POINT_READ_PAGE));

    /* 清除后不再触发 */
    fault_clear();
    EXPECT_FALSE(fault_should_fail(FAULT_POINT_READ_PAGE));

    /* 清除后可以重新注册 */
    EXPECT_EQ(fault_register(&cfg), 0);
    EXPECT_TRUE(fault_should_fail(FAULT_POINT_READ_PAGE));
}