/*
 * GC 垃圾回收器测试
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/delete/vector_delete_bitmap.h>
#include <db/index/vector_index/delete/vector_gc_vacuum.h>

class GCVacuumTest : public ::testing::Test {
protected:
    void SetUp() override {
        bitmap_ = vector_delete_bitmap_create(1024);
        ASSERT_NE(bitmap_, nullptr);

        gc_config_t config;
        gc_config_init_default(&config);
        config.auto_vacuum = false;  /* 关闭自动 GC */
        config.batch_size = 10;

        vacuum_ = gc_vacuum_create(bitmap_, &config);
        ASSERT_NE(vacuum_, nullptr);
    }

    void TearDown() override {
        if (vacuum_) {
            gc_vacuum_stop(vacuum_);
            gc_vacuum_destroy(vacuum_);
            vacuum_ = nullptr;
        }
        if (bitmap_) {
            vector_delete_bitmap_destroy(bitmap_);
            bitmap_ = nullptr;
        }
    }

    vector_delete_bitmap_t *bitmap_;
    gc_vacuum_t *vacuum_;
};

TEST_F(GCVacuumTest, CreateAndDestroy)
{
    EXPECT_NE(vacuum_, nullptr);
    EXPECT_EQ(gc_vacuum_get_state(vacuum_), GC_STATE_IDLE);
}

TEST_F(GCVacuumTest, ExecuteWithNoDeleted)
{
    /* 没有删除时，GC 应该返回 0 */
    int32_t result = gc_vacuum_execute(vacuum_, GC_SCAN_MODE_FULL, nullptr, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(GCVacuumTest, ExecuteWithDeleted)
{
    /* 标记一些删除 */
    int32_t deleted[] = {10, 20, 30, 40, 50};
    vector_delete_mark_batch(bitmap_, deleted, 5);

    /* 执行 GC */
    int32_t result = gc_vacuum_execute(vacuum_, GC_SCAN_MODE_FULL, nullptr, nullptr);
    EXPECT_EQ(result, 5);

    /* 验证删除标记被清空 */
    for (int32_t i = 0; i < 5; i++) {
        EXPECT_FALSE(vector_delete_is_deleted(bitmap_, deleted[i]));
    }
}

TEST_F(GCVacuumTest, ProgressCallback)
{
    /* 标记删除 */
    int32_t deleted[20];
    for (int32_t i = 0; i < 20; i++) {
        deleted[i] = i * 10;
    }
    vector_delete_mark_batch(bitmap_, deleted, 20);

    /* 跟踪回调调用 */
    int32_t last_processed = -1;

    auto callback = [](int32_t processed, int32_t /*total*/, void *ctx) {
        int32_t *last_processed = (int32_t *)ctx;
        *last_processed = processed;
    };

    int32_t result = gc_vacuum_execute(
        vacuum_, GC_SCAN_MODE_FULL, callback, &last_processed);

    EXPECT_EQ(result, 20);
    EXPECT_EQ(last_processed, 20);
}

TEST_F(GCVacuumTest, IncrementalScan)
{
    /* 标记第一批删除 */
    int32_t first_batch[] = {10, 20, 30};
    vector_delete_mark_batch(bitmap_, first_batch, 3);

    /* 第一次 GC */
    gc_vacuum_execute(vacuum_, GC_SCAN_MODE_INCREMENTAL, nullptr, nullptr);

    /* 标记第二批删除 */
    int32_t second_batch[] = {40, 50, 60};
    vector_delete_mark_batch(bitmap_, second_batch, 3);

    /* 第二次 GC（应该只扫描第二批） */
    gc_vacuum_execute(vacuum_, GC_SCAN_MODE_INCREMENTAL, nullptr, nullptr);

    /* 所有删除都应该被清空 */
    for (int32_t i = 0; i < 3; i++) {
        EXPECT_FALSE(vector_delete_is_deleted(bitmap_, first_batch[i]));
        EXPECT_FALSE(vector_delete_is_deleted(bitmap_, second_batch[i]));
    }
}

TEST_F(GCVacuumTest, GetStats)
{
    /* 标记删除并执行 GC */
    int32_t deleted[] = {10, 20, 30, 40, 50};
    vector_delete_mark_batch(bitmap_, deleted, 5);

    gc_vacuum_execute(vacuum_, GC_SCAN_MODE_FULL, nullptr, nullptr);

    /* 检查统计 */
    gc_stats_t stats;
    gc_vacuum_get_stats(vacuum_, &stats);

    EXPECT_EQ(stats.total_vacuumed, 5);
    EXPECT_GE(stats.total_time_us, 0);
}

TEST_F(GCVacuumTest, UpdateConfig)
{
    gc_config_t new_config;
    gc_config_init_default(&new_config);
    new_config.batch_size = 50;
    new_config.min_deleted_ratio = 20;

    EXPECT_EQ(gc_vacuum_update_config(vacuum_, &new_config), 0);
}

TEST_F(GCVacuumTest, StateTransitions)
{
    /* 初始状态 */
    EXPECT_EQ(gc_vacuum_get_state(vacuum_), GC_STATE_IDLE);

    /* 执行 GC */
    int32_t deleted[] = {10, 20};
    vector_delete_mark_batch(bitmap_, deleted, 2);
    gc_vacuum_execute(vacuum_, GC_SCAN_MODE_FULL, nullptr, nullptr);

    /* 执行后回到 IDLE */
    EXPECT_EQ(gc_vacuum_get_state(vacuum_), GC_STATE_IDLE);
}
