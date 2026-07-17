/*
 * 向量搜索结果过滤测试
 */

#include <gtest/gtest.h>
#include <db/index/vector_index/delete/vector_delete_bitmap.h>
#include <db/index/vector_index/delete/vector_search_filter.h>

class VectorSearchFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        bitmap_ = vector_delete_bitmap_create(1024);
        ASSERT_NE(bitmap_, nullptr);

        ctx_ = vector_search_filter_create(bitmap_, 10, 0.5f);
        ASSERT_NE(ctx_, nullptr);
    }

    void TearDown() override {
        if (ctx_) {
            vector_search_filter_destroy(ctx_);
            ctx_ = nullptr;
        }
        if (bitmap_) {
            vector_delete_bitmap_destroy(bitmap_);
            bitmap_ = nullptr;
        }
    }

    vector_delete_bitmap_t *bitmap_;
    vector_search_filter_ctx_t *ctx_;
};

TEST_F(VectorSearchFilterTest, NoDeleted)
{
    /* 没有删除时，所有结果都保留 */
    int32_t vec_id[] = {1, 2, 3, 4, 5};
    float distance[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    int32_t result = vector_search_filter_deleted(ctx_, vec_id, distance, 5);

    EXPECT_EQ(result, 5);
    EXPECT_EQ(vec_id[0], 1);
    EXPECT_EQ(vec_id[4], 5);
}

TEST_F(VectorSearchFilterTest, FilterDeleted)
{
    /* 标记一些向量为已删除 */
    vector_delete_mark(bitmap_, 2);
    vector_delete_mark(bitmap_, 4);

    int32_t vec_id[] = {1, 2, 3, 4, 5};
    float distance[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    int32_t result = vector_search_filter_deleted(ctx_, vec_id, distance, 5);

    /* 2 和 4 被过滤掉 */
    EXPECT_EQ(result, 3);
    EXPECT_EQ(vec_id[0], 1);
    EXPECT_EQ(vec_id[1], 3);
    EXPECT_EQ(vec_id[2], 5);
}

TEST_F(VectorSearchFilterTest, AllDeleted)
{
    /* 所有结果都被删除 */
    vector_delete_mark(bitmap_, 1);
    vector_delete_mark(bitmap_, 2);
    vector_delete_mark(bitmap_, 3);
    vector_delete_mark(bitmap_, 4);
    vector_delete_mark(bitmap_, 5);

    int32_t vec_id[] = {1, 2, 3, 4, 5};
    float distance[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    int32_t result = vector_search_filter_deleted(ctx_, vec_id, distance, 5);

    EXPECT_EQ(result, 0);
}

TEST_F(VectorSearchFilterTest, CheckDeleted)
{
    /* 测试 vector_check_deleted 辅助函数 */
    vector_delete_mark(bitmap_, 100);

    EXPECT_TRUE(vector_check_deleted(bitmap_, 100));
    EXPECT_FALSE(vector_check_deleted(bitmap_, 101));
    EXPECT_FALSE(vector_check_deleted(nullptr, 100));
}
