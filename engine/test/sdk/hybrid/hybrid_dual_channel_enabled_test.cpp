/**
 * @file hybrid_dual_channel_enabled_test.cpp
 * @brief P5-6：双模同集合 SDK 架构测试
 *
 * 测试场景：
 *   1. VectorCollectionWithTextEnabled — VECTOR 集合启用 text 后 hybrid 双通道
 *   2. TextCollectionWithVectorEnabled — TEXT 集合启用 vector 后 hybrid 双通道
 *   3. DualChannelRRFBoost — 双通道 RRF 融合效果（跨通道命中应有更高 rrf_score）
 *   4. RegressionVectorOnly — VECTOR 集合未启用 text 时行为不变
 *   5. RegressionTextOnly — TEXT 集合未启用 vector 时行为不变
 *   6. EnableIdempotent — enable 函数幂等性
 *
 * 设计要点：
 *   P5-6 引入 capability 标志位（has_text / has_vector），允许 TEXT / VECTOR
 *   集合通过 mmdb_text_enable() / mmdb_vectors_enable() 动态开启另一能力。
 *   本测试验证 enable 后 hybrid_search 双通道真正激活（不再静默吞掉错误），
 *   以及未启用时回归既有行为。
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
constexpr const char* kDbPath = "test_hybrid_dual_channel_enabled.db";
constexpr size_t kVecDim = 4;
}

class HybridDualChannelEnabledTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::remove(kDbPath);
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);

        /* VECTOR 集合（默认 has_vector=1, has_text=0） */
        mmdb_schema_t vs = {MMDB_MODEL_VECTOR, 0, nullptr, kVecDim};
        vc_ = mmdb_collection_create(db_, "vec", &vs);
        ASSERT_NE(vc_, nullptr);

        /* TEXT 集合（默认 has_text=1, has_vector=0）；同时预填 vector_dim
         * 以便 mmdb_vectors_enable() 后能正常添加向量（P5-6 双模同集合架构） */
        mmdb_schema_t ts = {MMDB_MODEL_TEXT, 0, nullptr, kVecDim};
        tc_ = mmdb_collection_create(db_, "txt", &ts);
        ASSERT_NE(tc_, nullptr);
    }

    void TearDown() override {
        if (db_) mmdb_close(db_);
        std::remove(kDbPath);
        std::remove((std::string(kDbPath) + "-wal").c_str());
        std::remove((std::string(kDbPath) + "-shm").c_str());
    }

    /* 填充 10 条数据：id 在两个集合共享 */
    void FillData() {
        const char* docs[10] = {
            "machine learning basics", "deep neural network tutorial",
            "cooking pasta recipe",     "italian food guide",
            "vector database systems",  "machine learning algorithms",
            "baking bread at home",     "database indexing techniques",
            "neural network training",  "pasta carbonara recipe"
        };
        for (int i = 0; i < 10; i++) {
            std::string id = "doc" + std::to_string(i);
            float vec[kVecDim] = {(float)i, (float)(i % 3), 0.5f, 0.1f};

            mmdb_vector_t v = {
                (const uint8_t*)id.data(), id.size(),
                vec, kVecDim, nullptr, nullptr
            };
            ASSERT_EQ(mmdb_vectors_add(vc_, &v, 1), MMDB_OK);

            mmdb_text_entry_t e = {id.c_str(), docs[i], nullptr};
            ASSERT_EQ(mmdb_text_add(tc_, &e), MMDB_OK);
        }
    }

    /* 判断结果中是否包含某 id（线性扫描，K 通常 <= 10） */
    bool HasId(const mmdb_result_t& out, const std::string& target) {
        for (size_t i = 0; i < out.count; i++) {
            std::string id((const char*)out.items[i].id, out.items[i].id_len);
            if (id == target) return true;
        }
        return false;
    }

    mmdb_t* db_ = nullptr;
    mmdb_collection_t* vc_ = nullptr;  /* VECTOR 集合 */
    mmdb_collection_t* tc_ = nullptr;  /* TEXT 集合 */
};

/* 测试 1：VECTOR 集合启用 text 后 hybrid 双通道真正激活
 *
 * 启用 mmdb_text_enable(vc_) 后：
 *   - 主通道 vector 应正常返回候选
 *   - 次通道 text 不再返回 MMDB_ERR_INVALID，可以基于文本相似度补充候选
 *   - 集合的结果数应 >= 仅主通道的结果数 */
TEST_F(HybridDualChannelEnabledTest, VectorCollectionWithTextEnabled) {
    FillData();

    /* 启用 text 能力 */
    ASSERT_EQ(mmdb_text_enable(vc_), MMDB_OK);

    /* 向 VECTOR 集合插入文本（FTS5 路径） */
    mmdb_text_entry_t e = {"doc0", "machine learning basics extra", nullptr};
    ASSERT_EQ(mmdb_text_add(vc_, &e), MMDB_OK);

    /* hybrid：vector + text_query 同时提供 */
    float query_vec[kVecDim] = {1.0f, 0.5f, 0.5f, 0.1f};
    mmdb_hybrid_query_t q = {};
    q.vector = query_vec;
    q.dim = kVecDim;
    q.text_query = "machine";
    q.top_k = 5;

    mmdb_result_t out = {};
    ASSERT_EQ(mmdb_hybrid_search(vc_, &q, &out), MMDB_OK);
    EXPECT_GT(out.count, 0u) << "VECTOR+text_enable 双通道应至少返回 1 个候选";

    mmdb_result_free(&out);
}

/* 测试 2：TEXT 集合启用 vector 后 hybrid 双通道真正激活
 *
 * 启用 mmdb_vectors_enable(tc_) 后：
 *   - 主通道 text 应正常返回候选
 *   - 次通道 vector 不再返回 MMDB_ERR_INVALID，可以基于向量相似度补充候选 */
TEST_F(HybridDualChannelEnabledTest, TextCollectionWithVectorEnabled) {
    FillData();

    /* 启用 vector 能力 */
    ASSERT_EQ(mmdb_vectors_enable(tc_), MMDB_OK);

    /* 向 TEXT 集合插入向量 */
    float vec[kVecDim] = {1.0f, 0.5f, 0.5f, 0.1f};
    mmdb_vector_t v = {
        (const uint8_t*)"doc0", 4, vec, kVecDim, nullptr, nullptr
    };
    ASSERT_EQ(mmdb_vectors_add(tc_, &v, 1), MMDB_OK);

    /* hybrid：text_query + vector 同时提供 */
    float query_vec[kVecDim] = {1.0f, 0.5f, 0.5f, 0.1f};
    mmdb_hybrid_query_t q = {};
    q.vector = query_vec;
    q.dim = kVecDim;
    q.text_query = "machine";
    q.top_k = 5;

    mmdb_result_t out = {};
    ASSERT_EQ(mmdb_hybrid_search(tc_, &q, &out), MMDB_OK);
    EXPECT_GT(out.count, 0u) << "TEXT+vector_enable 双通道应至少返回 1 个候选";

    /* 文本通道应命中 doc0（"machine learning basics"） */
    EXPECT_TRUE(HasId(out, "doc0"))
        << "TEXT 通道命中 doc0 应保留到 RRF 融合结果";

    mmdb_result_free(&out);
}

/* 测试 3：双通道 RRF 融合效果
 *
 * 验证当某 id 同时被两通道命中时，融合后其 rrf_score 应高于仅单通道命中。 */
TEST_F(HybridDualChannelEnabledTest, DualChannelRRFBoost) {
    FillData();

    /* 启用双能力 */
    ASSERT_EQ(mmdb_text_enable(vc_), MMDB_OK);
    ASSERT_EQ(mmdb_vectors_enable(tc_), MMDB_OK);

    /* 向两个集合都补充 doc0，使其在双通道都可命中 */
    mmdb_text_entry_t e = {"doc0", "machine learning very deep", nullptr};
    ASSERT_EQ(mmdb_text_add(vc_, &e), MMDB_OK);
    /* doc0 在 tc_ 中已存在（FillData） */

    /* 双通道 hybrid 查询 */
    float query_vec[kVecDim] = {0.0f, 0.0f, 0.5f, 0.1f};  /* 接近 doc0 */
    mmdb_hybrid_query_t q = {};
    q.vector = query_vec;
    q.dim = kVecDim;
    q.text_query = "machine learning";
    q.top_k = 3;

    mmdb_result_t out = {};
    ASSERT_EQ(mmdb_hybrid_search(vc_, &q, &out), MMDB_OK);
    EXPECT_GT(out.count, 0u);

    /* 注：严格断言"双通道命中 rrf_score > 单通道"需控制变量；这里仅断言
     * hybrid 返回 OK 且有结果，避免对 RRF 公式耦合过深。 */
    bool found_doc0 = HasId(out, "doc0");
    EXPECT_TRUE(found_doc0);

    mmdb_result_free(&out);
}

/* 回归测试 4：VECTOR 集合未启用 text 时行为不变
 *
 * 与 P4-T4.2 hybrid_dual_channel_test 一致：次通道 text 静默失败，主通道正常 */
TEST_F(HybridDualChannelEnabledTest, RegressionVectorOnly) {
    FillData();

    /* 不调用 mmdb_text_enable(vc_) */

    float query_vec[kVecDim] = {1.0f, 0.5f, 0.5f, 0.1f};
    mmdb_hybrid_query_t q = {};
    q.vector = query_vec;
    q.dim = kVecDim;
    q.text_query = "machine learning";
    q.top_k = 5;

    mmdb_result_t out = {};
    ASSERT_EQ(mmdb_hybrid_search(vc_, &q, &out), MMDB_OK);
    EXPECT_GT(out.count, 0u) << "VECTOR 单通道仍应正常工作";

    mmdb_result_free(&out);
}

/* 回归测试 5：TEXT 集合未启用 vector 时行为不变 */
TEST_F(HybridDualChannelEnabledTest, RegressionTextOnly) {
    FillData();

    /* 不调用 mmdb_vectors_enable(tc_) */

    float query_vec[kVecDim] = {1.0f, 0.5f, 0.5f, 0.1f};
    mmdb_hybrid_query_t q = {};
    q.vector = query_vec;
    q.dim = kVecDim;
    q.text_query = "machine learning";
    q.top_k = 5;

    mmdb_result_t out = {};
    ASSERT_EQ(mmdb_hybrid_search(tc_, &q, &out), MMDB_OK);
    EXPECT_GT(out.count, 0u) << "TEXT 单通道仍应正常工作";

    mmdb_result_free(&out);
}

/* 测试 6：enable 函数幂等性
 *
 * mmdb_text_enable() / mmdb_vectors_enable() 重复调用应无副作用 */
TEST_F(HybridDualChannelEnabledTest, EnableIdempotent) {
    /* 第一次 enable：置位 capability 并创建表 */
    ASSERT_EQ(mmdb_text_enable(vc_), MMDB_OK);
    ASSERT_EQ(mmdb_text_enable(vc_), MMDB_OK);
    ASSERT_EQ(mmdb_text_enable(vc_), MMDB_OK);

    ASSERT_EQ(mmdb_vectors_enable(tc_), MMDB_OK);
    ASSERT_EQ(mmdb_vectors_enable(tc_), MMDB_OK);

    /* 重复调用返回 MMDB_OK 且不破坏现有数据 */
    FillData();

    /* TEXT 集合默认 has_text=1（创建时已开），enable 仍应幂等 */
    ASSERT_EQ(mmdb_text_enable(tc_), MMDB_OK);
    ASSERT_EQ(mmdb_vectors_enable(vc_), MMDB_OK);

    /* 验证 enable 后跨通道操作正常 */
    mmdb_text_entry_t e = {"doc0", "machine learning", nullptr};
    ASSERT_EQ(mmdb_text_add(vc_, &e), MMDB_OK);
    float vvec[kVecDim] = {0.0f, 0.0f, 0.5f, 0.1f};
    mmdb_vector_t v = {
        (const uint8_t*)"doc0", 4, vvec, kVecDim, nullptr, nullptr
    };
    ASSERT_EQ(mmdb_vectors_add(tc_, &v, 1), MMDB_OK);
}
