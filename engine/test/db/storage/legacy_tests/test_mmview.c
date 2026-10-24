/**
 * @file test_mmview.c
 * @brief 物化视图测试
 *
 * 测试 mview_refresh_fast（增量刷新）和 mview_refresh_complete（全量刷新）
 */
#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 头文件 */
#include "db/storage/mmview/mview.h"
#include "db/storage/common/storage_result.h"

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

class MViewTest : public ::testing::Test {
protected:
    void SetUp() override {
        mview_init();
    }

    void TearDown() override {
        mview_shutdown();
    }
};

/* ========================================================================
 * 物化视图基本操作测试
 * ======================================================================== */

TEST_F(MViewTest, CreateAndGet) {
    uint32_t mview_id = 999;
    int ret = mview_create("test_view", "SELECT * FROM orders",
                          MVIEW_REFRESH_COMPLETE, &mview_id);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(mview_id, 999u);

    const MViewInfo *info = mview_get_info(mview_id);
    ASSERT_NE(info, nullptr);
    EXPECT_STREQ(info->name, "test_view");
    EXPECT_EQ(info->state, MVIEW_STATE_READY);
    EXPECT_EQ(info->refresh_type, MVIEW_REFRESH_COMPLETE);
}

TEST_F(MViewTest, CreateMultiple) {
    uint32_t id1, id2;
    mview_create("view1", "SELECT 1", MVIEW_REFRESH_COMPLETE, &id1);
    mview_create("view2", "SELECT 2", MVIEW_REFRESH_FAST, &id2);

    EXPECT_NE(id1, id2);

    const MViewInfo *info1 = mview_get_info(id1);
    const MViewInfo *info2 = mview_get_info(id2);
    ASSERT_NE(info1, nullptr);
    ASSERT_NE(info2, nullptr);
    EXPECT_EQ(info1->refresh_type, MVIEW_REFRESH_COMPLETE);
    EXPECT_EQ(info2->refresh_type, MVIEW_REFRESH_FAST);
}

TEST_F(MViewTest, GetInvalid) {
    EXPECT_EQ(mview_get_info(9999), nullptr);
    EXPECT_EQ(mview_get_info((uint32_t)-1), nullptr);
}

/* ========================================================================
 * 全量刷新测试
 * ======================================================================== */

TEST_F(MViewTest, RefreshCompleteBasic) {
    uint32_t mview_id;
    mview_create("test_view", "SELECT * FROM orders",
                MVIEW_REFRESH_COMPLETE, &mview_id);

    int ret = mview_refresh_complete(mview_id);
    EXPECT_EQ(ret, STORAGE_OK);

    const MViewInfo *info = mview_get_info(mview_id);
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->state, MVIEW_STATE_READY);
    EXPECT_GT(info->last_refresh_time, 0u);
}

TEST_F(MViewTest, RefreshCompleteInvalidId) {
    int ret = mview_refresh_complete(9999);
    EXPECT_EQ(ret, STORAGE_ERR_INVALID_ARG);
}

TEST_F(MViewTest, RefreshCompleteWithQuery) {
    uint32_t mview_id;
    mview_create("sales_view", "SELECT region, SUM(amount) FROM sales GROUP BY region",
                MVIEW_REFRESH_COMPLETE, &mview_id);

    int ret = mview_refresh_complete(mview_id);
    EXPECT_EQ(ret, STORAGE_OK);

    const MViewInfo *info = mview_get_info(mview_id);
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->state, MVIEW_STATE_READY);
    /* 查询被执行后应该有行数统计 */
    EXPECT_EQ(info->row_count, 100u);  /* 模拟结果行数 */
}

/* ========================================================================
 * 增量刷新测试
 * ======================================================================== */

TEST_F(MViewTest, RefreshFastBasic) {
    uint32_t mview_id;
    mview_create("test_view", "SELECT * FROM orders",
                MVIEW_REFRESH_FAST, &mview_id);

    /* 先添加基础表依赖 */
    mview_add_base_dependency(mview_id, 1000);

    int ret = mview_refresh_fast(mview_id);
    EXPECT_EQ(ret, STORAGE_OK);

    const MViewInfo *info = mview_get_info(mview_id);
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->state, MVIEW_STATE_READY);
    EXPECT_GT(info->last_refresh_time, 0u);
}

TEST_F(MViewTest, RefreshFastWrongType) {
    uint32_t mview_id;
    /* 创建为 COMPLETE 类型 */
    mview_create("test_view", "SELECT * FROM orders",
                MVIEW_REFRESH_COMPLETE, &mview_id);

    /* 尝试 FAST 刷新应该失败 */
    int ret = mview_refresh_fast(mview_id);
    EXPECT_EQ(ret, STORAGE_ERR_INVALID_ARG);
}

TEST_F(MViewTest, RefreshFastWithChanges) {
    uint32_t mview_id;
    mview_create("test_view", "SELECT * FROM orders",
                MVIEW_REFRESH_FAST, &mview_id);

    mview_add_base_dependency(mview_id, 1000);

    /* 初始刷新 */
    mview_refresh_fast(mview_id);

    const MViewInfo *info_before = mview_get_info(mview_id);
    ASSERT_NE(info_before, nullptr);
    uint32_t rows_before = info_before->row_count;

    /* 记录一些变更 */
    uint32_t data1[4] = {1, 100, 5000, 0};
    uint32_t data2[4] = {2, 101, 3000, 0};
    uint32_t data3[4] = {1, 100, 5000, 0};  /* 用于删除 */

    mview_record_change(1000, 'I', data1, sizeof(data1));  /* INSERT */
    mview_record_change(1000, 'U', data2, sizeof(data2));  /* UPDATE */
    mview_record_change(1000, 'D', data3, sizeof(data3));  /* DELETE */

    /* 再次刷新 */
    int ret = mview_refresh_fast(mview_id);
    EXPECT_EQ(ret, STORAGE_OK);

    const MViewInfo *info_after = mview_get_info(mview_id);
    ASSERT_NE(info_after, nullptr);
    /* INSERT 增加 1，DELETE 减少 1，UPDATE 不变 */
    EXPECT_EQ(info_after->row_count, rows_before);
}

TEST_F(MViewTest, RefreshFastInvalidId) {
    int ret = mview_refresh_fast(9999);
    EXPECT_EQ(ret, STORAGE_ERR_INVALID_ARG);
}

/* ========================================================================
 * CDC 变更捕获测试
 * ======================================================================== */

TEST_F(MViewTest, RecordAndGetChanges) {
    uint32_t mview_id;
    mview_create("test_view", "SELECT * FROM orders",
                MVIEW_REFRESH_FAST, &mview_id);

    mview_add_base_dependency(mview_id, 2000);

    /* 记录变更 */
    uint32_t data1[4] = {1, 100, 5000, 0};
    uint32_t data2[4] = {2, 101, 3000, 0};

    mview_record_change(2000, 'I', data1, sizeof(data1));
    mview_record_change(2000, 'U', data2, sizeof(data2));

    /* 获取变更 */
    MViewChange changes[10];
    int count = mview_get_changes(2000, changes, 10);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(changes[0].change_type, 'I');
    EXPECT_EQ(changes[1].change_type, 'U');
}

TEST_F(MViewTest, AcknowledgeChanges) {
    uint32_t mview_id;
    mview_create("test_view", "SELECT * FROM orders",
                MVIEW_REFRESH_FAST, &mview_id);

    mview_add_base_dependency(mview_id, 3000);

    /* 记录变更 */
    uint32_t data[4] = {1, 100, 5000, 0};
    mview_record_change(3000, 'I', data, sizeof(data));
    mview_record_change(3000, 'I', data, sizeof(data));
    mview_record_change(3000, 'I', data, sizeof(data));

    /* 确认变更 */
    MViewChange changes[10];
    int count_before = mview_get_changes(3000, changes, 10);
    EXPECT_EQ(count_before, 3);

    mview_acknowledge_changes(3000, changes[1].change_id);

    /* 再次获取应该只剩 1 个 */
    int count_after = mview_get_changes(3000, changes, 10);
    EXPECT_EQ(count_after, 1);
}

TEST_F(MViewTest, ClearChanges) {
    uint32_t mview_id;
    mview_create("test_view", "SELECT * FROM orders",
                MVIEW_REFRESH_FAST, &mview_id);

    mview_add_base_dependency(mview_id, 4000);

    /* 记录变更 */
    uint32_t data[4] = {1, 100, 5000, 0};
    mview_record_change(4000, 'I', data, sizeof(data));
    mview_record_change(4000, 'I', data, sizeof(data));

    /* 清除变更 */
    mview_clear_changes(4000);

    MViewChange changes[10];
    int count = mview_get_changes(4000, changes, 10);
    EXPECT_EQ(count, 0);
}

/* ========================================================================
 * 依赖图测试
 * ======================================================================== */

TEST_F(MViewTest, AddDependency) {
    uint32_t id1, id2;
    mview_create("view1", "SELECT 1", MVIEW_REFRESH_COMPLETE, &id1);
    mview_create("view2", "SELECT 2", MVIEW_REFRESH_COMPLETE, &id2);

    int ret = mview_add_dependency(id2, id1);
    EXPECT_EQ(ret, 0);

    /* mview_add_dependency 递增 mview_id（依赖方）的 refcount */
    const MViewInfo *info = mview_get_info(id2);
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->refcount, 1u);
}

TEST_F(MViewTest, AddBaseDependency) {
    uint32_t mview_id;
    mview_create("test_view", "SELECT * FROM orders",
                MVIEW_REFRESH_FAST, &mview_id);

    int ret = mview_add_base_dependency(mview_id, 5000);
    EXPECT_EQ(ret, 0);
}

TEST_F(MViewTest, CycleDetection) {
    uint32_t id1, id2, id3;
    mview_create("view1", "SELECT 1", MVIEW_REFRESH_COMPLETE, &id1);
    mview_create("view2", "SELECT 2", MVIEW_REFRESH_COMPLETE, &id2);
    mview_create("view3", "SELECT 3", MVIEW_REFRESH_COMPLETE, &id3);

    /* 建立依赖链: id1 <- id2 <- id3 */
    mview_add_dependency(id2, id1);
    mview_add_dependency(id3, id2);

    /* 无环 */
    EXPECT_FALSE(mview_has_cycle());

    /* 建立环: id1 <- id2 <- id3 <- id1 */
    mview_add_dependency(id1, id3);

    /* 有环 */
    EXPECT_TRUE(mview_has_cycle());
}

TEST_F(MViewTest, RefreshOrder) {
    uint32_t id1, id2, id3;
    mview_create("view1", "SELECT 1", MVIEW_REFRESH_COMPLETE, &id1);
    mview_create("view2", "SELECT 2", MVIEW_REFRESH_COMPLETE, &id2);
    mview_create("view3", "SELECT 3", MVIEW_REFRESH_COMPLETE, &id3);

    /* 建立依赖链: id1 <- id2 <- id3 */
    mview_add_dependency(id2, id1);
    mview_add_dependency(id3, id2);

    /* 获取刷新顺序 */
    uint32_t order[10];
    int count = mview_get_refresh_order(order, 10);
    EXPECT_EQ(count, 3);

    /* id1 应该在最前，id3 最后 */
    EXPECT_EQ(order[0], id1);
    EXPECT_EQ(order[2], id3);
}

/* ========================================================================
 * 刷新调度测试
 * ======================================================================== */

TEST_F(MViewTest, ScheduleSetAndGet) {
    uint32_t mview_id;
    mview_create("test_view", "SELECT * FROM orders",
                MVIEW_REFRESH_COMPLETE, &mview_id);

    MViewSchedule schedule;
    memset(&schedule, 0, sizeof(schedule));
    schedule.mview_id = mview_id;
    schedule.policy = MVIEW_SCHEDULE_INTERVAL;
    schedule.interval_ms = 60000;
    schedule.enabled = true;

    int ret = mview_set_schedule(mview_id, &schedule);
    EXPECT_EQ(ret, 0);

    const MViewSchedule *got = mview_get_schedule(mview_id);
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(got->policy, MVIEW_SCHEDULE_INTERVAL);
    EXPECT_EQ(got->interval_ms, 60000u);
    EXPECT_TRUE(got->enabled);
}

TEST_F(MViewTest, ScheduleEnableDisable) {
    uint32_t mview_id;
    mview_create("test_view", "SELECT * FROM orders",
                MVIEW_REFRESH_COMPLETE, &mview_id);

    mview_enable_schedule(mview_id);
    const MViewSchedule *s1 = mview_get_schedule(mview_id);
    ASSERT_NE(s1, nullptr);
    EXPECT_TRUE(s1->enabled);

    mview_disable_schedule(mview_id);
    const MViewSchedule *s2 = mview_get_schedule(mview_id);
    ASSERT_NE(s2, nullptr);
    EXPECT_FALSE(s2->enabled);
}

/* ========================================================================
 * 统计测试
 * ======================================================================== */

TEST_F(MViewTest, Stats) {
    MViewStats stats_before, stats_after;

    mview_get_stats(&stats_before);

    /* 创建视图并刷新 */
    uint32_t mview_id;
    mview_create("test_view", "SELECT * FROM orders",
                MVIEW_REFRESH_COMPLETE, &mview_id);
    mview_refresh_complete(mview_id);

    mview_get_stats(&stats_after);

    EXPECT_GT(stats_after.total_mviews, stats_before.total_mviews);
    EXPECT_GT(stats_after.total_refreshes, stats_before.total_refreshes);
}

TEST_F(MViewTest, Dump) {
    /* 不崩溃即可 */
    mview_dump();
    mview_dump_graph();
}
