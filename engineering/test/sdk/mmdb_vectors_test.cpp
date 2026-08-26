// mmdb_vectors_test.cpp — Task 8：向量模型（Flat KNN）测试
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "sdk/mmdb.h"
#include "sdk/mmdb_vectors.h"

namespace {
constexpr const char* kDbPath = "test_mmdb_vectors.db";
constexpr size_t kDim = 4;
}

class MmdbVectorsTest : public ::testing::Test {
   protected:
    mmdb_t* db_ = nullptr;
    mmdb_collection_t* coll_ = nullptr;

    void SetUp() override {
        std::remove(kDbPath);
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);
        mmdb_schema_t s = {MMDB_MODEL_VECTOR, 0, nullptr, kDim};
        coll_ = mmdb_collection_create(db_, "vec", &s);
        ASSERT_NE(coll_, nullptr);
    }
    void TearDown() override {
        if (db_) mmdb_close(db_);
        std::remove(kDbPath);
    }
};

TEST_F(MmdbVectorsTest, AddAndSearchSingleVector) {
    const char* id = "v1";
    float vec[kDim] = {1.0f, 0.0f, 0.0f, 0.0f};
    mmdb_vector_t v = {(const uint8_t*)id, 2, vec, kDim, nullptr, nullptr};
    EXPECT_EQ(mmdb_vectors_add(coll_, &v, 1), MMDB_OK);

    float q[kDim] = {1.0f, 0.0f, 0.0f, 0.0f};
    mmdb_query_t query = {q, kDim, 5, nullptr};
    mmdb_result_t result;
    EXPECT_EQ(mmdb_vectors_search(coll_, &query, &result), MMDB_OK);
    EXPECT_EQ(result.count, 1u);
    EXPECT_EQ(result.items[0].id_len, 2u);
    EXPECT_EQ(memcmp(result.items[0].id, "v1", 2), 0);
    EXPECT_FLOAT_EQ(result.items[0].distance, 0.0f);
    mmdb_result_free(&result);
}

TEST_F(MmdbVectorsTest, KnnReturnsClosestFirst) {
    const char* ids[] = {"a", "b", "c", "d"};
    float vecs[][kDim] = {
        {1.0f, 0.0f, 0.0f, 0.0f},   /* a */
        {0.0f, 1.0f, 0.0f, 0.0f},   /* b */
        {0.0f, 0.0f, 1.0f, 0.0f},   /* c */
        {0.5f, 0.5f, 0.5f, 0.5f},   /* d */
    };
    for (int i = 0; i < 4; i++) {
        mmdb_vector_t v = {(const uint8_t*)ids[i], 1, vecs[i], kDim, nullptr,
                           nullptr};
        ASSERT_EQ(mmdb_vectors_add(coll_, &v, 1), MMDB_OK);
    }

    /* 查询接近 a 的向量 */
    float q[kDim] = {1.0f, 0.1f, 0.0f, 0.0f};
    mmdb_query_t query = {q, kDim, 2, nullptr};
    mmdb_result_t result;
    ASSERT_EQ(mmdb_vectors_search(coll_, &query, &result), MMDB_OK);
    ASSERT_GE(result.count, 2u);
    /* 最近的是 a */
    EXPECT_EQ(memcmp(result.items[0].id, "a", 1), 0);
    EXPECT_FLOAT_EQ(result.items[0].distance, 0.01f);
    mmdb_result_free(&result);
}

TEST_F(MmdbVectorsTest, BatchInsert) {
    const char* ids[] = {"x", "y", "z"};
    float vecs[][kDim] = {
        {0.1f, 0.2f, 0.3f, 0.4f},
        {0.5f, 0.6f, 0.7f, 0.8f},
        {0.9f, 0.0f, 0.1f, 0.2f},
    };
    std::vector<mmdb_vector_t> batch;
    for (int i = 0; i < 3; i++) {
        batch.push_back({(const uint8_t*)ids[i], 1, vecs[i], kDim, nullptr,
                         nullptr});
    }
    EXPECT_EQ(mmdb_vectors_add(coll_, batch.data(), 3), MMDB_OK);
}

TEST_F(MmdbVectorsTest, UpsertReplacesById) {
    const char* id = "k";
    float v1[kDim] = {1.0f, 0.0f, 0.0f, 0.0f};
    float v2[kDim] = {0.0f, 0.0f, 0.0f, 1.0f};
    mmdb_vector_t a = {(const uint8_t*)id, 1, v1, kDim, nullptr, nullptr};
    EXPECT_EQ(mmdb_vectors_add(coll_, &a, 1), MMDB_OK);

    mmdb_vector_t b = {(const uint8_t*)id, 1, v2, kDim, nullptr, nullptr};
    EXPECT_EQ(mmdb_vectors_upsert(coll_, &b, 1), MMDB_OK);

    /* 查询应返回 v2 */
    float q[kDim] = {0.0f, 0.0f, 0.0f, 1.0f};
    mmdb_query_t query = {q, kDim, 1, nullptr};
    mmdb_result_t result;
    ASSERT_EQ(mmdb_vectors_search(coll_, &query, &result), MMDB_OK);
    EXPECT_FLOAT_EQ(result.items[0].distance, 0.0f);
    mmdb_result_free(&result);
}

TEST_F(MmdbVectorsTest, DeleteRemovesVector) {
    const char* id = "z";
    float v[kDim] = {1.0f, 0.0f, 0.0f, 0.0f};
    mmdb_vector_t vec = {(const uint8_t*)id, 1, v, kDim, nullptr, nullptr};
    ASSERT_EQ(mmdb_vectors_add(coll_, &vec, 1), MMDB_OK);

    EXPECT_EQ(mmdb_vectors_delete(coll_, (const uint8_t*)id, 1), MMDB_OK);

    float q[kDim] = {1.0f, 0.0f, 0.0f, 0.0f};
    mmdb_query_t query = {q, kDim, 10, nullptr};
    mmdb_result_t result;
    ASSERT_EQ(mmdb_vectors_search(coll_, &query, &result), MMDB_OK);
    EXPECT_EQ(result.count, 0u);
    mmdb_result_free(&result);
}

TEST_F(MmdbVectorsTest, WrongCollectionModelFails) {
    mmdb_schema_t gs = {MMDB_MODEL_GRAPH, 0, nullptr, 0};
    mmdb_collection_t* g = mmdb_collection_create(db_, "g", &gs);
    ASSERT_NE(g, nullptr);

    const char* id = "x";
    float v[kDim] = {1.0f, 0.0f, 0.0f, 0.0f};
    mmdb_vector_t vec = {(const uint8_t*)id, 1, v, kDim, nullptr, nullptr};
    EXPECT_NE(mmdb_vectors_add(g, &vec, 1), MMDB_OK);
}

TEST_F(MmdbVectorsTest, EmptyCollectionReturnsEmptyResults) {
    float q[kDim] = {1.0f, 0.0f, 0.0f, 0.0f};
    mmdb_query_t query = {q, kDim, 10, nullptr};
    mmdb_result_t result;
    ASSERT_EQ(mmdb_vectors_search(coll_, &query, &result), MMDB_OK);
    EXPECT_EQ(result.count, 0u);
    mmdb_result_free(&result);
}

TEST_F(MmdbVectorsTest, DimMismatchRejected) {
    const char* id = "bad";
    float v[2] = {1.0f, 0.0f};
    mmdb_vector_t vec = {(const uint8_t*)id, 3, v, 2, nullptr, nullptr};
    EXPECT_NE(mmdb_vectors_add(coll_, &vec, 1), MMDB_OK);
}