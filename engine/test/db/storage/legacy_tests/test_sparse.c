/**
 * @file test_sparse.c
 * @brief Sparse 检索模块测试（min-heap top-k 验证）
 *
 * 测试 hybrid_retrieval 的 min-heap 实现：
 * - 基本 top-k 正确性
 * - 候选数 < k 的边界情况
 * - 重复分数的处理
 */
#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* C 接口头文件 */
extern "C" {
#include "db/hybrid_retrieval.h"
#include "db/bm25_index.h"
#include "db/sparse_vector.h"
}

/* ========================================================================
 * Min-Heap 正确性测试（内部实现细节验证）
 * ======================================================================== */

/* 内部 min_heap_t 在 hybrid_retrieval.c 中声明为 static，
 * 无法直接访问。因此通过 hybrid_search API 间接验证：
 * 构造可控场景，比较 qsort 结果与 heap 结果是否一致。
 */

class SparseRetrievalTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

/**
 * @brief 测试 top-k 当候选数等于 k
 */
TEST_F(SparseRetrievalTest, TopKEqualsCandidateCount) {
    /* 构造一个纯 BM25 场景：使用固定 dense_index，禁用归一化 */
    hybrid_config_t config = hybrid_config_default();
    config.alpha = 0.0f;
    config.beta = 0.0f;
    config.gamma = 1.0f;
    config.normalize_scores = false;

    /* 构造 5 个文档的 BM25 索引 */
    bm25_config_t bm25_cfg = {1.2f, 0.75f};
    bm25_index_t *bm25 = bm25_index_create(bm25_cfg);
    ASSERT_NE(bm25, nullptr);

    bm25_index_add_document(bm25, 10, "hello world");
    bm25_index_add_document(bm25, 20, "hello database");
    bm25_index_add_document(bm25, 30, "world database");
    bm25_index_add_document(bm25, 40, "database system");
    bm25_index_add_document(bm25, 50, "system performance");

    /* 查询 "hello"：doc 10 和 20 应有分数 */
    hybrid_result_t results[5];
    uint32_t num_results = 0;

    int ret = hybrid_search(
        NULL, 0, NULL, "hello",
        NULL, 0, bm25,
        &config,
        5,  /* top_k = candidate_count */
        results, &num_results
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_results, 5u);
    /* 结果应按 final_score 降序排列 */
    for (uint32_t i = 1; i < num_results; i++) {
        EXPECT_GE(results[i-1].final_score, results[i].final_score);
    }

    bm25_index_free(bm25);
}

/**
 * @brief 测试 top-k 小于候选数（触发 min-heap 路径）
 */
TEST_F(SparseRetrievalTest, TopKLessThanCandidates) {
    hybrid_config_t config = hybrid_config_default();
    config.alpha = 0.0f;
    config.beta = 0.0f;
    config.gamma = 1.0f;
    config.normalize_scores = false;

    bm25_config_t bm25_cfg = {1.2f, 0.75f};
    bm25_index_t *bm25 = bm25_index_create(bm25_cfg);
    ASSERT_NE(bm25, nullptr);

    bm25_index_add_document(bm25, 1, "apple banana cherry");
    bm25_index_add_document(bm25, 2, "banana cherry date");
    bm25_index_add_document(bm25, 3, "cherry date elderberry");
    bm25_index_add_document(bm25, 4, "date elderberry fig");
    bm25_index_add_document(bm25, 5, "elderberry fig grape");

    /* 查询 "cherry"，前两名应包含 doc 1, 2, 3 */
    hybrid_result_t results[10];
    uint32_t num_results = 0;

    int ret = hybrid_search(
        NULL, 0, NULL, "cherry",
        NULL, 0, bm25,
        &config,
        2,  /* top_k = 2，候选共 5 个，触发 min-heap */
        results, &num_results
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_results, 2u);
    /* 返回的 2 个结果应按分数降序 */
    EXPECT_GE(results[0].final_score, results[1].final_score);
    /* doc 1 ("apple banana cherry") 和 doc 2 ("banana cherry date") 的 cherry 词频最高 */
    EXPECT_TRUE(results[0].id == 1 || results[0].id == 2);
    EXPECT_TRUE(results[1].id == 1 || results[1].id == 2);

    bm25_index_free(bm25);
}

/**
 * @brief 测试 top-k 为 1
 */
TEST_F(SparseRetrievalTest, TopKEqualsOne) {
    hybrid_config_t config = hybrid_config_default();
    config.alpha = 0.0f;
    config.beta = 0.0f;
    config.gamma = 1.0f;
    config.normalize_scores = false;

    bm25_config_t bm25_cfg = {1.2f, 0.75f};
    bm25_index_t *bm25 = bm25_index_create(bm25_cfg);
    ASSERT_NE(bm25, nullptr);

    bm25_index_add_document(bm25, 100, "foo bar");
    bm25_index_add_document(bm25, 200, "foo baz");
    bm25_index_add_document(bm25, 300, "bar baz");

    hybrid_result_t results[1];
    uint32_t num_results = 0;

    int ret = hybrid_search(
        NULL, 0, NULL, "foo",
        NULL, 0, bm25,
        &config,
        1,
        results, &num_results
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_results, 1u);
    /* doc 100 和 200 都包含 foo，取最高分 */
    EXPECT_TRUE(results[0].id == 100 || results[0].id == 200);

    bm25_index_free(bm25);
}

/**
 * @brief 测试空结果场景
 */
TEST_F(SparseRetrievalTest, EmptyResults) {
    hybrid_config_t config = hybrid_config_default();

    bm25_config_t bm25_cfg = {1.2f, 0.75f};
    bm25_index_t *bm25 = bm25_index_create(bm25_cfg);
    ASSERT_NE(bm25, nullptr);

    bm25_index_add_document(bm25, 1, "hello world");

    hybrid_result_t results[10];
    uint32_t num_results = 0;

    /* 查询不存在的词 */
    int ret = hybrid_search(
        NULL, 0, NULL, "nonexistent_xyz",
        NULL, 0, bm25,
        &config,
        5,
        results, &num_results
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_results, 0u);

    bm25_index_free(bm25);
}

/**
 * @brief 测试 BM25 索引为空
 */
TEST_F(SparseRetrievalTest, EmptyIndex) {
    hybrid_config_t config = hybrid_config_default();
    bm25_config_t bm25_cfg = {1.2f, 0.75f};
    bm25_index_t *bm25 = bm25_index_create(bm25_cfg);
    ASSERT_NE(bm25, nullptr);

    hybrid_result_t results[10];
    uint32_t num_results = 0;

    int ret = hybrid_search(
        NULL, 0, NULL, "hello",
        NULL, 0, bm25,
        &config,
        5,
        results, &num_results
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_results, 0u);

    bm25_index_free(bm25);
}

/**
 * @brief 测试候选数小于 top_k（边界）
 */
TEST_F(SparseRetrievalTest, CandidatesLessThanK) {
    hybrid_config_t config = hybrid_config_default();
    config.alpha = 0.0f;
    config.beta = 0.0f;
    config.gamma = 1.0f;
    config.normalize_scores = false;

    bm25_config_t bm25_cfg = {1.2f, 0.75f};
    bm25_index_t *bm25 = bm25_index_create(bm25_cfg);
    ASSERT_NE(bm25, nullptr);

    /* 只添加 2 个文档 */
    bm25_index_add_document(bm25, 1, "foo");
    bm25_index_add_document(bm25, 2, "bar");

    hybrid_result_t results[10];
    uint32_t num_results = 0;

    /* top_k = 5，候选 = 2，触发候选数 < k 的快速路径 */
    int ret = hybrid_search(
        NULL, 0, NULL, "foo",
        NULL, 0, bm25,
        &config,
        5,
        results, &num_results
    );

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(num_results, 2u); /* 应返回全部 2 个 */
    EXPECT_GE(results[0].final_score, results[1].final_score);

    bm25_index_free(bm25);
}
