// rag_test.cpp — P3-T4.1：RAG retrieve 单元测试
//
// 测试场景：
//   1. RetrieveContext   — 检索 + 拼接 + items 数量正确 + context 含预期文本
//   2. ContextTruncation — max_context_chars 截断生效
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
constexpr const char* kDbPath = "test_rag.db";
constexpr size_t kDim = 64;  /* text collection 不强制，但 embedding 接口要求 */
}

class RagTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::remove(kDbPath);
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);

        /* RAG 用 text collection（schema 中 vector_dim 不强制） */
        mmdb_schema_t ts = {MMDB_MODEL_TEXT, 0, nullptr, kDim};
        tc_ = mmdb_collection_create(db_, "rag_docs", &ts);
        ASSERT_NE(tc_, nullptr);

        /* 插入 50 段文本 */
        const char* docs[5] = {
            "machine learning basics and introduction to neural networks",
            "deep learning tutorial for beginners with python examples",
            "cooking pasta recipe with tomato sauce and basil",
            "vector database systems and similarity search algorithms",
            "natural language processing with transformer models"
        };
        for (int i = 0; i < 50; i++) {
            std::string id = "d" + std::to_string(i);
            mmdb_text_entry_t e = {
                id.c_str(), docs[i % 5], nullptr
            };
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

TEST_F(RagTest, RetrieveContext) {
    mmdb_rag_query_t q = {};
    q.query_text = "machine learning";
    q.top_k = 5;
    q.max_context_chars = 8000;

    mmdb_rag_result_t r = {};
    ASSERT_EQ(mmdb_rag_retrieve(tc_, &q, &r), MMDB_OK);

    /* context 必须非空 */
    EXPECT_NE(r.context, nullptr);
    EXPECT_GT(r.context_len, 0);

    /* context_len 严格 <= max_context_chars */
    EXPECT_LE(r.context_len, q.max_context_chars);

    /* items 应有 top_k 个结果 */
    EXPECT_EQ(r.items.count, q.top_k);

    /* context 应至少包含一段 "machine learning" 文本 */
    EXPECT_NE(std::string(r.context, r.context_len).find("machine learning"),
              std::string::npos);

    mmdb_rag_result_free(&r);
}

TEST_F(RagTest, ContextTruncation) {
    mmdb_rag_query_t q = {};
    q.query_text = "deep learning";
    q.top_k = 50;  /* 全部文档 */
    q.max_context_chars = 100;  /* 强制截断 */

    mmdb_rag_result_t r = {};
    ASSERT_EQ(mmdb_rag_retrieve(tc_, &q, &r), MMDB_OK);

    EXPECT_LE(r.context_len, q.max_context_chars);

    mmdb_rag_result_free(&r);
}
