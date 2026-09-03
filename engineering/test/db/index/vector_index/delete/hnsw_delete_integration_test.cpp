/*
 * HNSW 删除功能集成测试
 */

#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <db/index/vector_index/hnsw/faiss_hnsw.h>
#include <db/index/vector_index/delete/vector_delete_bitmap.h>

class HNSWDeleteIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 创建 HNSW 索引 */
        index_ = faiss_hnsw_index_create(
            16,      /* M */
            128,     /* dims */
            200,     /* ef_construction */
            DISTANCE_METRIC_L2_SQUARED,
            QUANTIZATION_TYPE_NONE);
        ASSERT_NE(index_, nullptr);

        /* 插入一些向量 */
        std::vector<float> vectors(100 * 128);
        for (int32_t i = 0; i < 100; i++) {
            for (int32_t d = 0; d < 128; d++) {
                vectors[i * 128 + d] = static_cast<float>(i + d) / 100.0f;
            }
        }
        ASSERT_EQ(faiss_hnsw_index_add(index_, 100, vectors.data()), 0);
    }

    void TearDown() override {
        if (index_) {
            faiss_hnsw_index_drop(index_);
            index_ = nullptr;
        }
    }

    faiss_hnsw_t *index_;
};

TEST_F(HNSWDeleteIntegrationTest, DeleteAndQuery)
{
    /* 删除向量 10 */
    EXPECT_EQ(faiss_hnsw_index_delete(index_, 10), 0);

    /* 查询删除状态 */
    EXPECT_TRUE(faiss_hnsw_index_is_deleted(index_, 10));
    EXPECT_FALSE(faiss_hnsw_index_is_deleted(index_, 11));

    /* 搜索应该能正常工作 */
    std::vector<float> query(128, 10.0f);
    std::vector<int32_t> vec_id(10);
    std::vector<float> distance(10);

    int32_t result = faiss_hnsw_index_search(
        index_, query.data(), 10, 50, distance.data(), vec_id.data());

    EXPECT_GT(result, 0);

    /* 结果中不应包含已删除的向量 10 */
    for (int32_t i = 0; i < result; i++) {
        EXPECT_NE(vec_id[i], 10);
    }
}

TEST_F(HNSWDeleteIntegrationTest, BatchDelete)
{
    /* 批量删除 */
    int32_t ids[] = {5, 10, 15, 20, 25};
    EXPECT_EQ(faiss_hnsw_index_delete_batch(index_, ids, 5), 5);

    /* 验证删除状态 */
    for (int32_t i = 0; i < 5; i++) {
        EXPECT_TRUE(faiss_hnsw_index_is_deleted(index_, ids[i]));
    }
}

TEST_F(HNSWDeleteIntegrationTest, Undelete)
{
    /* 删除 */
    EXPECT_EQ(faiss_hnsw_index_delete(index_, 30), 0);
    EXPECT_TRUE(faiss_hnsw_index_is_deleted(index_, 30));

    /* 撤销删除 */
    EXPECT_EQ(faiss_hnsw_index_undelete(index_, 30), 0);
    EXPECT_FALSE(faiss_hnsw_index_is_deleted(index_, 30));
}

TEST_F(HNSWDeleteIntegrationTest, InvalidDelete)
{
    /* 无效的删除请求 */
    EXPECT_EQ(faiss_hnsw_index_delete(index_, -1), -1);
    EXPECT_EQ(faiss_hnsw_index_delete(index_, 999), -1);
    EXPECT_EQ(faiss_hnsw_index_delete(nullptr, 0), -1);
}

TEST_F(HNSWDeleteIntegrationTest, DeleteAll)
{
    /* 删除所有向量 */
    for (int32_t i = 0; i < 100; i++) {
        EXPECT_EQ(faiss_hnsw_index_delete(index_, i), 0);
    }

    /* 所有向量都应该是已删除状态 */
    for (int32_t i = 0; i < 100; i++) {
        EXPECT_TRUE(faiss_hnsw_index_is_deleted(index_, i));
    }

    /* 搜索应该返回 0 */
    std::vector<float> query(128, 0.0f);
    std::vector<int32_t> vec_id(10);
    std::vector<float> distance(10);

    int32_t result = faiss_hnsw_index_search(
        index_, query.data(), 10, 50, distance.data(), vec_id.data());

    EXPECT_EQ(result, 0);
}

TEST_F(HNSWDeleteIntegrationTest, DeleteAndUndelete)
{
    /* 删除后再恢复一些 */
    int32_t ids[] = {1, 2, 3, 4, 5};
    faiss_hnsw_index_delete_batch(index_, ids, 5);

    /* 恢复部分 */
    faiss_hnsw_index_undelete(index_, 2);
    faiss_hnsw_index_undelete(index_, 4);

    EXPECT_FALSE(faiss_hnsw_index_is_deleted(index_, 2));
    EXPECT_FALSE(faiss_hnsw_index_is_deleted(index_, 4));
    EXPECT_TRUE(faiss_hnsw_index_is_deleted(index_, 1));
    EXPECT_TRUE(faiss_hnsw_index_is_deleted(index_, 3));
    EXPECT_TRUE(faiss_hnsw_index_is_deleted(index_, 5));
}
