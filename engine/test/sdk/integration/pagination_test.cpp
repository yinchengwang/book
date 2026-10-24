/**
 * @file pagination_test.cpp
 * @brief P6-M1.1 分页 API 测试
 *
 * 测试 mmdb_vectors_search() 的 offset/limit 分页行为以及
 * mmdb_result_t 的 total_count / has_more / returned 字段。
 *
 * 约束：
 *  - top_k 设为足够大（100），确保所有 100 条向量都能返回
 *  - 5 个测试用例覆盖：首页 / 第二页 / 末页 / 越界 / 向后兼容
 */
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "sdk/mmdb.h"
#include "sdk/mmdb_vectors.h"

namespace {

constexpr const char* kDbPath = "test_pagination.db";
constexpr size_t kDim = 16;        /* 测试向量维度（轻量级） */
constexpr size_t kNumVectors = 100; /* 用于分页测试的向量数 */

/* 生成 ID 字符串（"v0", "v1", ..., "v99"） */
std::string make_id(size_t i) {
    return "v" + std::to_string(i);
}

/* 生成第 i 条向量（线性变化模式，确保距离各不相同） */
std::vector<float> make_vector(size_t i) {
    std::vector<float> v(kDim);
    for (size_t j = 0; j < kDim; j++) {
        v[j] = static_cast<float>(i * kDim + j) / 1000.0f;
    }
    return v;
}

/* 清理数据库文件（含 WAL/SHM） */
void cleanup_db() {
    std::remove(kDbPath);
    std::remove((std::string(kDbPath) + "-wal").c_str());
    std::remove((std::string(kDbPath) + "-shm").c_str());
}

}  // namespace

class PaginationTest : public ::testing::Test {
   protected:
    mmdb_t* db_ = nullptr;
    mmdb_collection_t* coll_ = nullptr;
    /* 简单的查询向量（全 0.5）；所有数据向量与它距离各不相同 */
    std::vector<float> query_vec_;

    void SetUp() override {
        cleanup_db();
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);

        mmdb_schema_t schema = {MMDB_MODEL_VECTOR, 0, nullptr, kDim};
        coll_ = mmdb_collection_create(db_, "test_pagination", &schema);
        ASSERT_NE(coll_, nullptr);

        /* 插入 kNumVectors 条向量 */
        for (size_t i = 0; i < kNumVectors; i++) {
            std::string id = make_id(i);
            std::vector<float> vec = make_vector(i);
            mmdb_vector_t v = {
                reinterpret_cast<const uint8_t*>(id.data()), id.size(),
                vec.data(), kDim, nullptr, nullptr};
            ASSERT_EQ(mmdb_vectors_add(coll_, &v, 1), MMDB_OK);
        }

        query_vec_.assign(kDim, 0.5f);
    }

    void TearDown() override {
        if (db_) mmdb_close(db_);
        cleanup_db();
    }
};

/* ================================================================== */
/* 测试 1：第一页（offset=0, limit=10）                                  */
/* ================================================================== */
TEST_F(PaginationTest, FirstPage) {
    mmdb_query_t query = {0};
    query.query_vector = query_vec_.data();
    query.dim = kDim;
    query.top_k = kNumVectors; /* 获取全部候选 */
    query.offset = 0;
    query.limit = 10;

    mmdb_result_t result;
    ASSERT_EQ(mmdb_vectors_search(coll_, &query, &result), MMDB_OK);

    /* 前 10 条 */
    EXPECT_EQ(result.returned, 10u);
    EXPECT_EQ(result.count, 10u);
    /* total_count 反映分页前的总数（top_k 范围内） */
    EXPECT_EQ(result.total_count, static_cast<uint32_t>(kNumVectors));
    /* 还有 90 条未返回 */
    EXPECT_TRUE(result.has_more);

    mmdb_result_free(&result);
}

/* ================================================================== */
/* 测试 2：第二页（offset=10, limit=10）                                */
/* ================================================================== */
TEST_F(PaginationTest, SecondPage) {
    mmdb_query_t query = {0};
    query.query_vector = query_vec_.data();
    query.dim = kDim;
    query.top_k = kNumVectors;
    query.offset = 10;
    query.limit = 10;

    mmdb_result_t result;
    ASSERT_EQ(mmdb_vectors_search(coll_, &query, &result), MMDB_OK);

    EXPECT_EQ(result.returned, 10u);
    EXPECT_EQ(result.count, 10u);
    EXPECT_EQ(result.total_count, static_cast<uint32_t>(kNumVectors));
    EXPECT_TRUE(result.has_more);

    mmdb_result_free(&result);
}

/* ================================================================== */
/* 测试 3：最后一页（offset=90, limit=10）                              */
/* ================================================================== */
TEST_F(PaginationTest, LastPage) {
    mmdb_query_t query = {0};
    query.query_vector = query_vec_.data();
    query.dim = kDim;
    query.top_k = kNumVectors;
    query.offset = 90;
    query.limit = 10;

    mmdb_result_t result;
    ASSERT_EQ(mmdb_vectors_search(coll_, &query, &result), MMDB_OK);

    EXPECT_EQ(result.returned, 10u);
    EXPECT_EQ(result.count, 10u);
    EXPECT_EQ(result.total_count, static_cast<uint32_t>(kNumVectors));
    /* 最后一页已无更多 */
    EXPECT_FALSE(result.has_more);

    mmdb_result_free(&result);
}

/* ================================================================== */
/* 测试 4：越界页（offset=95, limit=10，只剩 5 条）                      */
/* ================================================================== */
TEST_F(PaginationTest, OverflowPage) {
    mmdb_query_t query = {0};
    query.query_vector = query_vec_.data();
    query.dim = kDim;
    query.top_k = kNumVectors;
    query.offset = 95;
    query.limit = 10;

    mmdb_result_t result;
    ASSERT_EQ(mmdb_vectors_search(coll_, &query, &result), MMDB_OK);

    /* 只返回剩余的 5 条 */
    EXPECT_EQ(result.returned, 5u);
    EXPECT_EQ(result.count, 5u);
    EXPECT_EQ(result.total_count, static_cast<uint32_t>(kNumVectors));
    /* 全部返回完毕 */
    EXPECT_FALSE(result.has_more);

    mmdb_result_free(&result);
}

/* ================================================================== */
/* 测试 5：向后兼容（offset=0, limit=0，行为与旧版一致）                */
/* ================================================================== */
TEST_F(PaginationTest, BackwardCompatible) {
    mmdb_query_t query = {0};
    query.query_vector = query_vec_.data();
    query.dim = kDim;
    query.top_k = 10;        /* 旧版默认 top_k=10 */
    query.offset = 0;        /* 显式置 0 */
    query.limit = 0;         /* 显式置 0 */

    mmdb_result_t result;
    ASSERT_EQ(mmdb_vectors_search(coll_, &query, &result), MMDB_OK);

    /* 旧版行为：count = top_k（最多 10 条） */
    EXPECT_EQ(result.count, 10u);
    /* 新字段被填上，但语义与旧版一致 */
    EXPECT_EQ(result.returned, 10u);
    EXPECT_EQ(result.total_count, 10u);
    EXPECT_FALSE(result.has_more);

    mmdb_result_free(&result);
}

/* ================================================================== */
/* 测试 6：完全越界（offset 超过 total_count）                          */
/* ================================================================== */
TEST_F(PaginationTest, OffsetBeyondTotal) {
    mmdb_query_t query = {0};
    query.query_vector = query_vec_.data();
    query.dim = kDim;
    query.top_k = kNumVectors;
    query.offset = 200;      /* 远超总数 100 */
    query.limit = 10;

    mmdb_result_t result;
    ASSERT_EQ(mmdb_vectors_search(coll_, &query, &result), MMDB_OK);

    EXPECT_EQ(result.returned, 0u);
    EXPECT_EQ(result.count, 0u);
    EXPECT_EQ(result.total_count, static_cast<uint32_t>(kNumVectors));
    EXPECT_FALSE(result.has_more);

    mmdb_result_free(&result);
}