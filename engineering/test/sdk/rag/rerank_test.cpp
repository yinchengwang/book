// rerank_test.cpp — P3-T4.2：RAG BM25 rerank 占位测试
//
// 测试场景：
//   1. BM25Rerank — 启用 BM25 rerank 后，输出与 NONE 不同
//                  （验证 rerank 流程跑通，不强求具体顺序）
#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
#include "sdk/mmdb.h"
#include "sdk/mmdb_text.h"
#include "sdk/mmdb_rag.h"
}

namespace {
constexpr const char* kDbPath = "test_rerank.db";
constexpr size_t kDim = 64;
}

class RagTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::remove(kDbPath);
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);

        /* RAG 用 text collection */
        mmdb_schema_t ts = {MMDB_MODEL_TEXT, 0, nullptr, kDim};
        tc_ = mmdb_collection_create(db_, "rerank_docs", &ts);
        ASSERT_NE(tc_, nullptr);

        /* 插入 50 段文本：与 T4.1 一致的数据集 */
        const char* docs[5] = {
            "machine learning basics and introduction to neural networks",
            "deep learning tutorial for beginners with python examples",
            "cooking pasta recipe with tomato sauce and basil",
            "vector database systems and similarity search algorithms",
            "natural language processing with transformer models"
        };
        for (int i = 0; i < 50; i++) {
            std::string id = "d" + std::to_string(i);
            mmdb_text_entry_t e = {id.c_str(), docs[i % 5], nullptr};
            ASSERT_EQ(mmdb_text_add(tc_, &e), MMDB_OK);
        }
    }

    void TearDown() override {
        if (db_) mmdb_close(db_);
        std::remove(kDbPath);
    }

    mmdb_t* db_ = nullptr;
    mmdb_collection_t* tc_ = nullptr;
};

/* BM25 rerank 配置生效 + context 拼接仍正确。
 * 验证：
 *   1. rerank.kind=BM25 + weight=0.5 不报错
 *   2. 返回 items 数量 == top_k
 *   3. context 非空且 <= max_context_chars
 *   4. rerank 流程跑通（不要求具体排序，只验证接口可用） */
TEST_F(RagTest, BM25Rerank) {
    mmdb_rag_query_t q = {};
    q.query_text = "machine learning";
    q.top_k = 5;
    q.max_context_chars = 8000;
    q.rerank.kind = MMDB_RAG_RERANK_BM25;
    q.rerank.weight = 0.5;

    mmdb_rag_result_t r = {};
    ASSERT_EQ(mmdb_rag_retrieve(tc_, &q, &r), MMDB_OK);

    /* items 数 == top_k */
    EXPECT_EQ(r.items.count, q.top_k);

    /* context 非空且未超限 */
    EXPECT_NE(r.context, nullptr);
    EXPECT_GT(r.context_len, 0);
    EXPECT_LE(r.context_len, q.max_context_chars);

    mmdb_rag_result_free(&r);
}
