/**
 * @file rag_embedding_test.cpp
 * @brief P4-T4.1：RAG embedding 配置入口测试
 *
 * 测试场景：
 *   1. CustomEmbeddingTakesPrecedence — 注入 collection-level embedding 后，
 *      retrieve 使用该 embedding（而非默认 HASH）
 */
#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
#include "sdk/mmdb.h"
#include "sdk/mmdb_text.h"
#include "sdk/mmdb_rag.h"
#include "sdk/mmdb_embedding.h"
}

namespace {
constexpr const char* kDbPath = "test_rag_embedding.db";
constexpr size_t kDim = 64;
}

class RagEmbeddingTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::remove(kDbPath);
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);

        /* RAG 用 text collection */
        mmdb_schema_t ts = {MMDB_MODEL_TEXT, 0, nullptr, kDim};
        tc_ = mmdb_collection_create(db_, "emb_docs", &ts);
        ASSERT_NE(tc_, nullptr);

        /* 插入若干文本（足以让 hybrid search 有候选） */
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

TEST_F(RagEmbeddingTest, CustomEmbeddingTakesPrecedence) {
    /* 构造自定义 embedding（与 schema 同维度） */
    mmdb_embedding_t* my_emb = mmdb_embedding_create(MMDB_EMBED_HASH, kDim);
    ASSERT_NE(my_emb, nullptr);

    /* 注入到 collection */
    ASSERT_EQ(mmdb_rag_set_embedding(tc_, my_emb), MMDB_OK);

    /* per-call embedding 留 NULL，应 fallback 到 collection-level */
    mmdb_rag_query_t q = {0};
    q.query_text = "machine learning";
    q.top_k = 5;
    q.max_context_chars = 1000;

    mmdb_rag_result_t r = {0};
    ASSERT_EQ(mmdb_rag_retrieve(tc_, &q, &r), MMDB_OK);

    EXPECT_NE(r.context, nullptr);
    EXPECT_GT(r.context_len, 0u);
    EXPECT_LE(r.context_len, q.max_context_chars);
    EXPECT_EQ(r.items.count, q.top_k);

    mmdb_rag_result_free(&r);

    /* 所有权提示：collection 仅持指针，调用方负责 destroy */
    mmdb_embedding_drop(my_emb);
}