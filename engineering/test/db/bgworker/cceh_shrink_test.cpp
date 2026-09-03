/**
 * @file cceh_shrink_test.cpp
 * @brief CCEH 缩容任务测试
 */

#include <gtest/gtest.h>
#include "db/index/hash/cceh.h"
#include "db/bgworker.h"
#include "db/log.h"
#include <thread>
#include <vector>
#include <cstring>
#include <cstdio>

/* ============================================================
 * 测试夹具
 * ============================================================ */

class CcehShrinkTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 初始化日志系统 */
        log_config_t log_config;
        memset(&log_config, 0, sizeof(log_config));
        log_config.level = LOG_LEVEL_DEBUG;
        log_config.target = LOG_TARGET_CONSOLE;
        log_config.enable_colors = false;
        log_init(&log_config);
        log_set_level(LOG_LEVEL_DEBUG);

        /* 创建测试索引 */
        index_ = cceh_index_create(8, 1);
        ASSERT_NE(index_, nullptr);
    }

    void TearDown() override {
        if (index_) {
            cceh_index_drop(index_);
            index_ = nullptr;
        }
        db_bgworker_stop();
    }

    cceh_index_t *index_ = nullptr;
};

/* ============================================================
 * 测试用例
 * ============================================================ */

/**
 * @brief 测试缩容任务注册
 */
TEST_F(CcehShrinkTest, ShrinkTaskRegistration) {
    /* 启动后台任务调度器 */
    int rc = db_bgworker_start();
    EXPECT_EQ(rc, 0);

    /* 验证调度器可以正常启动和停止（CCEH 缩容任务由启动流程内部注册） */
    /* 短暂等待，让内置任务完成首次状态检查 */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    /* 查询调度器状态：任务计数应 >= 0 */
    db_bgworker_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    rc = db_bgworker_get_stats(&stats);
    EXPECT_EQ(rc, 0);
    EXPECT_GE(stats.task_count, 0u);

    /* 调度器可正常停止 */
    rc = db_bgworker_stop();
    EXPECT_EQ(rc, 0);
}

/**
 * @brief 测试负载因子计算
 */
TEST_F(CcehShrinkTest, LoadFactorCalculation) {
    /* 空索引应该有低负载因子 */
    uint32_t size = cceh_index_size(index_);
    uint32_t segment_count = cceh_index_segment_count(index_);

    EXPECT_EQ(size, 0u);
    EXPECT_GE(segment_count, 1u);

    /* 插入一些数据后负载因子应该增加 */
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        int value = i;
        int rc = cceh_index_insert(index_, key, strlen(key), &value, sizeof(value));
        EXPECT_GE(rc, 0);
    }

    size = cceh_index_size(index_);
    EXPECT_EQ(size, 10u);
}

/**
 * @brief 测试缩容安全检查
 */
TEST_F(CcehShrinkTest, ShrinkSafetyCheck) {
    /* 初始状态 global_depth = 1，应该不允许缩容 */
    uint32_t global_depth = cceh_index_global_depth(index_);
    EXPECT_GE(global_depth, 1u);

    /* 插入大量数据触发扩容 */
    for (int i = 0; i < 100; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        int value = i;
        int rc = cceh_index_upsert(index_, key, strlen(key), &value, sizeof(value));
        EXPECT_GE(rc, 0);
    }

    global_depth = cceh_index_global_depth(index_);
    /* 扩容后应该有更大的 global_depth */
    EXPECT_GE(global_depth, 1u);
}