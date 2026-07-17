/*
 * 向量删除标记位图单元测试
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/delete/vector_delete_bitmap.h>

class VectorDeleteBitmapTest : public ::testing::Test {
protected:
    void SetUp() override {
        bitmap_ = vector_delete_bitmap_create(1024);
        ASSERT_NE(bitmap_, nullptr);
    }

    void TearDown() override {
        if (bitmap_) {
            vector_delete_bitmap_destroy(bitmap_);
            bitmap_ = nullptr;
        }
    }

    vector_delete_bitmap_t *bitmap_;
};

TEST_F(VectorDeleteBitmapTest, CreateAndDestroy)
{
    /* 基本创建/销毁在 SetUp/TearDown 中测试 */
    EXPECT_NE(bitmap_, nullptr);
}

TEST_F(VectorDeleteBitmapTest, InitialState)
{
    /* 初始状态应该没有删除 */
    EXPECT_FALSE(vector_delete_is_deleted(bitmap_, 0));
    EXPECT_FALSE(vector_delete_is_deleted(bitmap_, 100));
    EXPECT_FALSE(vector_delete_is_deleted(bitmap_, 1000));

    vector_delete_stats_t stats;
    ASSERT_EQ(vector_delete_get_stats(bitmap_, &stats), 0);
    EXPECT_EQ(stats.deleted_count, 0);
    EXPECT_EQ(stats.deleted_ratio, 0.0f);
}

TEST_F(VectorDeleteBitmapTest, MarkAndQuery)
{
    /* 标记删除 */
    EXPECT_EQ(vector_delete_mark(bitmap_, 0), 0);
    EXPECT_EQ(vector_delete_mark(bitmap_, 100), 0);
    EXPECT_EQ(vector_delete_mark(bitmap_, 1000), 0);

    /* 查询删除状态 */
    EXPECT_TRUE(vector_delete_is_deleted(bitmap_, 0));
    EXPECT_TRUE(vector_delete_is_deleted(bitmap_, 100));
    EXPECT_TRUE(vector_delete_is_deleted(bitmap_, 1000));
    EXPECT_FALSE(vector_delete_is_deleted(bitmap_, 1));
    EXPECT_FALSE(vector_delete_is_deleted(bitmap_, 999));
}

TEST_F(VectorDeleteBitmapTest, Unmark)
{
    /* 先标记再撤销 */
    ASSERT_EQ(vector_delete_mark(bitmap_, 42), 0);
    ASSERT_TRUE(vector_delete_is_deleted(bitmap_, 42));

    EXPECT_EQ(vector_delete_unmark(bitmap_, 42), 0);
    EXPECT_FALSE(vector_delete_is_deleted(bitmap_, 42));
}

TEST_F(VectorDeleteBitmapTest, BatchDelete)
{
    int32_t ids[] = {10, 20, 30, 40, 50};
    constexpr int32_t n = 5;

    /* 批量标记 */
    EXPECT_EQ(vector_delete_mark_batch(bitmap_, ids, n), n);

    /* 验证所有都标记成功 */
    for (int32_t i = 0; i < n; i++) {
        EXPECT_TRUE(vector_delete_is_deleted(bitmap_, ids[i]));
    }

    /* 统计检查 */
    vector_delete_stats_t stats;
    ASSERT_EQ(vector_delete_get_stats(bitmap_, &stats), 0);
    EXPECT_EQ(stats.deleted_count, n);
}

TEST_F(VectorDeleteBitmapTest, BatchUnmark)
{
    /* 先批量标记 */
    int32_t ids[] = {10, 20, 30, 40, 50};
    constexpr int32_t n = 5;
    vector_delete_mark_batch(bitmap_, ids, n);

    /* 批量撤销 */
    EXPECT_EQ(vector_delete_unmark_batch(bitmap_, ids, n), n);

    /* 验证所有都已撤销 */
    for (int32_t i = 0; i < n; i++) {
        EXPECT_FALSE(vector_delete_is_deleted(bitmap_, ids[i]));
    }
}

TEST_F(VectorDeleteBitmapTest, GetDeletedIds)
{
    /* 标记多个 */
    int32_t deleted[] = {5, 10, 15, 20, 25};
    constexpr int32_t n = 5;
    vector_delete_mark_batch(bitmap_, deleted, n);

    /* 获取已删除 ID 列表 */
    int32_t output[10];
    int32_t count = vector_delete_get_deleted_ids(bitmap_, output, 10);

    EXPECT_EQ(count, n);
    for (int32_t i = 0; i < count; i++) {
        bool found = false;
        for (int32_t j = 0; j < n; j++) {
            if (output[i] == deleted[j]) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found);
    }
}

TEST_F(VectorDeleteBitmapTest, Clear)
{
    /* 标记多个 */
    int32_t ids[] = {10, 20, 30};
    vector_delete_mark_batch(bitmap_, ids, 3);

    /* 清空 */
    vector_delete_clear(bitmap_);

    /* 验证都已清除 */
    for (int32_t i = 0; i < 3; i++) {
        EXPECT_FALSE(vector_delete_is_deleted(bitmap_, ids[i]));
    }

    /* 统计检查 */
    vector_delete_stats_t stats;
    ASSERT_EQ(vector_delete_get_stats(bitmap_, &stats), 0);
    EXPECT_EQ(stats.deleted_count, 0);
}

TEST_F(VectorDeleteBitmapTest, InvalidInput)
{
    /* 空指针 */
    EXPECT_EQ(vector_delete_mark(nullptr, 0), -1);
    EXPECT_EQ(vector_delete_unmark(nullptr, 0), -1);
    EXPECT_FALSE(vector_delete_is_deleted(nullptr, 0));

    /* 负数 ID */
    EXPECT_EQ(vector_delete_mark(bitmap_, -1), -1);
    EXPECT_EQ(vector_delete_unmark(bitmap_, -1), -1);
    EXPECT_FALSE(vector_delete_is_deleted(bitmap_, -1));
}

TEST_F(VectorDeleteBitmapTest, AutoExpand)
{
    /* 标记超出初始容量的 ID */
    EXPECT_EQ(vector_delete_mark(bitmap_, 2000), 0);
    EXPECT_TRUE(vector_delete_is_deleted(bitmap_, 2000));
}

TEST_F(VectorDeleteBitmapTest, DoubleMark)
{
    /* 多次标记同一 ID */
    EXPECT_EQ(vector_delete_mark(bitmap_, 100), 0);
    EXPECT_EQ(vector_delete_mark(bitmap_, 100), 0);
    EXPECT_TRUE(vector_delete_is_deleted(bitmap_, 100));

    /* 统计应该只计一次 */
    vector_delete_stats_t stats;
    ASSERT_EQ(vector_delete_get_stats(bitmap_, &stats), 0);
    EXPECT_EQ(stats.deleted_count, 1);
}

TEST_F(VectorDeleteBitmapTest, DefaultCapacity)
{
    /* 测试默认容量创建 */
    vector_delete_bitmap_t *default_bitmap = vector_delete_bitmap_create(0);
    ASSERT_NE(default_bitmap, nullptr);

    /* 应该能正常工作 */
    EXPECT_EQ(vector_delete_mark(default_bitmap, 0), 0);
    EXPECT_TRUE(vector_delete_is_deleted(default_bitmap, 0));

    vector_delete_bitmap_destroy(default_bitmap);
}
