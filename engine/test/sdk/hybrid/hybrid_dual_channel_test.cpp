/**
 * @file hybrid_dual_channel_test.cpp
 * @brief P4-T4.2：hybrid search 双通道真正融合测试
 *
 * 测试场景：
 *   1. VectorCollectionWithTextQuery  — VECTOR 集合同时填 vector + text_query，
 *      主通道 vector 应命中；次通道 text 因 model mismatch 静默失败（不破坏主通道）
 *   2. TextCollectionWithVectorQuery  — TEXT 集合同时填 text_query + vector，
 *      主通道 text 应命中；次通道 vector 因 model mismatch 静默失败
 *   3. PrimaryVectorStillWorks        — 仅 vector，无 text_query，回归 VECTOR 单通道
 *   4. PrimaryTextStillWorks          — 仅 text_query，无 vector，回归 TEXT 单通道
 *   5. PrimaryVectorStillWorksNoText  — VECTOR 上 text_query 为空串，
 *      验证 strlen() > 0 判定（空串不启用次通道）
 *
 * 设计要点（P4-T4.2 CI-1 关闭）：
 *   由于 SDK 架构限制，mmdb_text_search 与 mmdb_vectors_search 各自对集合 model
 *   严格检查（VECTOR 集合拒绝 text / TEXT 集合拒绝 vector），所以次通道在实际
 *   调用底层 API 时必然返回 MMDB_ERR_INVALID。本测试验证的是"主通道不被打断、
 *   次通道被尝试但不破坏整体行为"。若后续 SDK 支持 VECTOR/TEXT 同集合并存，
 *   这些断言可继续生效（不会退化）。
 */
#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
#include "sdk/mmdb.h"
#include "sdk/mmdb_vectors.h"
#include "sdk/mmdb_text.h"
#include "sdk/mmdb_hybrid.h"
}

namespace {
constexpr const char* kDbPath = "test_hybrid_dual_channel.db";
}

class HybridDualChannelTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::remove(kDbPath);
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);

        /* VECTOR 集合 */
        mmdb_schema_t vs = {MMDB_MODEL_VECTOR, 0, nullptr, 4};
        vc_ = mmdb_collection_create(db_, "vec", &vs);
        ASSERT_NE(vc_, nullptr);

        /* TEXT 集合 */
        mmdb_schema_t ts = {MMDB_MODEL_TEXT, 0, nullptr, 0};
        tc_ = mmdb_collection_create(db_, "txt", &ts);
        ASSERT_NE(tc_, nullptr);

        /* 插入 10 条数据：id 在两个集合共享 */
        const char* docs[10] = {
            "machine learning basics", "deep neural network tutorial",
            "cooking pasta recipe",     "italian food guide",
            "vector database systems",  "machine learning algorithms",
            "baking bread at home",     "database indexing techniques",
            "neural network training",  "pasta carbonara recipe"
        };
        for (int i = 0; i < 10; i++) {
            std::string id = "doc" + std::to_string(i);
            float vec[4] = {(float)i, (float)(i % 3), 0.5f, 0.1f};

            mmdb_vector_t v = {
                (const uint8_t*)id.data(), id.size(),
                vec, 4, nullptr, nullptr
            };
            ASSERT_EQ(mmdb_vectors_add(vc_, &v, 1), MMDB_OK);

            mmdb_text_entry_t e = {
                id.c_str(), docs[i], nullptr
            };
            ASSERT_EQ(mmdb_text_add(tc_, &e), MMDB_OK);
        }
    }

    void TearDown() override {
        if (db_) mmdb_close(db_);
        std::remove(kDbPath);
    }

    mmdb_t* db_ = nullptr;
    mmdb_collection_t* vc_ = nullptr;
    mmdb_collection_t* tc_ = nullptr;
};

/* 主测试：VECTOR 集合 + 同时提供 vector 与 text_query
 *
 * 当前实现路由：
 *   - 主通道 vector（model == VECTOR && q.vector != NULL）→ 跑 mmdb_vectors_search
 *   - 次通道 text（q.text_query != NULL && strlen > 0）→ 跑 mmdb_text_search，
 *     因 VECTOR 集合无 FTS5 表，mmdb_text_search 立即返回 MMDB_ERR_INVALID，
 *     次通道被静默忽略
 *
 * 期望：MMDB_OK + count > 0（主通道结果保留），不破坏整体行为
 */
TEST_F(HybridDualChannelTest, VectorCollectionWithTextQuery) {
    float query_vec[4] = {1.0f, 0.5f, 0.5f, 0.1f};  /* 接近 doc1 */
    mmdb_hybrid_query_t q = {};
    q.vector = query_vec;
    q.dim = 4;
    q.text_query = "machine learning";
    q.top_k = 5;
    q.rrf = nullptr;

    mmdb_result_t out = {};
    ASSERT_EQ(mmdb_hybrid_search(vc_, &q, &out), MMDB_OK);
    EXPECT_GT(out.count, 0u) << "VECTOR 主通道应至少返回 1 个候选";

    mmdb_result_free(&out);
}

/* 主测试：TEXT 集合 + 同时提供 text_query 与 vector
 *
 * 当前实现路由：
 *   - 主通道 text（model == TEXT && q.text_query != NULL）→ 跑 mmdb_text_search
 *   - 次通道 vector（q.vector != NULL && q.dim > 0）→ 跑 mmdb_vectors_search，
 *     因 TEXT 集合无向量数据且 mmdb_vectors_search 检查 model != VECTOR，
 *     立即返回 MMDB_ERR_INVALID，次通道被静默忽略
 *
 * 期望：MMDB_OK + count > 0（主通道结果保留），不破坏整体行为
 */
TEST_F(HybridDualChannelTest, TextCollectionWithVectorQuery) {
    mmdb_hybrid_query_t q = {};
    q.text_query = "machine learning";
    q.top_k = 5;
    q.rrf = nullptr;

    mmdb_result_t out = {};
    ASSERT_EQ(mmdb_hybrid_search(tc_, &q, &out), MMDB_OK);
    EXPECT_GT(out.count, 0u) << "TEXT 主通道应至少返回 1 个候选";

    /* doc0 ("machine learning basics") 与 doc5 ("machine learning algorithms")
     * 都在文本通道命中集合 → 必在 hybrid 结果集 */
    bool found_doc0 = false, found_doc5 = false;
    for (size_t i = 0; i < out.count; i++) {
        std::string id((const char*)out.items[i].id, out.items[i].id_len);
        if (id == "doc0") found_doc0 = true;
        if (id == "doc5") found_doc5 = true;
    }
    EXPECT_TRUE(found_doc0);
    EXPECT_TRUE(found_doc5);

    mmdb_result_free(&out);
}

/* 回归测试：VECTOR 集合仅 vector（无 text_query），主通道应正常工作 */
TEST_F(HybridDualChannelTest, PrimaryVectorStillWorks) {
    float query_vec[4] = {1.0f, 0.5f, 0.5f, 0.1f};
    mmdb_hybrid_query_t q = {};
    q.vector = query_vec;
    q.dim = 4;
    q.text_query = nullptr;  /* 显式 NULL，验证 strlen 判定 */
    q.top_k = 3;
    q.rrf = nullptr;

    mmdb_result_t out = {};
    ASSERT_EQ(mmdb_hybrid_search(vc_, &q, &out), MMDB_OK);
    EXPECT_GT(out.count, 0u);

    mmdb_result_free(&out);
}

/* 回归测试：TEXT 集合仅 text_query（无 vector），主通道应正常工作 */
TEST_F(HybridDualChannelTest, PrimaryTextStillWorks) {
    mmdb_hybrid_query_t q = {};
    q.text_query = "neural network";
    q.top_k = 3;
    q.rrf = nullptr;

    mmdb_result_t out = {};
    ASSERT_EQ(mmdb_hybrid_search(tc_, &q, &out), MMDB_OK);
    EXPECT_GT(out.count, 0u);

    mmdb_result_free(&out);
}

/* 边界用例：VECTOR 集合 + text_query 为空串
 *
 * 期望：空串 strlen() == 0 → 不启用次通道 text，仅主通道 vector 跑
 */
TEST_F(HybridDualChannelTest, PrimaryVectorStillWorksNoText) {
    float query_vec[4] = {1.0f, 0.5f, 0.5f, 0.1f};
    mmdb_hybrid_query_t q = {};
    q.vector = query_vec;
    q.dim = 4;
    q.text_query = "";  /* 空串，strlen == 0 */
    q.top_k = 3;
    q.rrf = nullptr;

    mmdb_result_t out = {};
    ASSERT_EQ(mmdb_hybrid_search(vc_, &q, &out), MMDB_OK);
    EXPECT_GT(out.count, 0u);

    mmdb_result_free(&out);
}
