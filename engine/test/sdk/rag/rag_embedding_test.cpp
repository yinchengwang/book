/**
 * @file rag_embedding_test.cpp
 * @brief P4-T4.1：RAG embedding 配置入口测试
 *
 * 测试场景：
 *   1. CustomEmbeddingTakesPrecedence — 注入 collection-level embedding 后，
 *      retrieve 使用该 embedding（而非默认 HASH）
 *   2. NullCollectionReturnsInvalid — 传 NULL collection 给 set_embedding 应报错
 *   3. RepeatedSetEmbedding — 连续多次 set_embedding 应返回 OK（后者覆盖前者）
 *   4. NullEmbeddingClearsCollectionLevel — set_embedding(coll, NULL) 应清空
 *      collection-level embedding，retrieve 走 HASH fallback
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

/* 负向用例：NULL collection 应返回 MMDB_ERR_INVALID */
TEST_F(RagEmbeddingTest, NullCollectionReturnsInvalid) {
    mmdb_embedding_t* emb = mmdb_embedding_create(MMDB_EMBED_HASH, kDim);
    ASSERT_NE(emb, nullptr);
    EXPECT_EQ(mmdb_rag_set_embedding(nullptr, emb), MMDB_ERR_INVALID);
    mmdb_embedding_drop(emb);
}

/* 负向用例：连续多次 set_embedding 不报错，第二次覆盖前者 */
TEST_F(RagEmbeddingTest, RepeatedSetEmbedding) {
    mmdb_embedding_t* emb_a = mmdb_embedding_create(MMDB_EMBED_HASH, kDim);
    mmdb_embedding_t* emb_b = mmdb_embedding_create(MMDB_EMBED_HASH, kDim);
    ASSERT_NE(emb_a, nullptr);
    ASSERT_NE(emb_b, nullptr);

    /* 两次设置都应成功（API 契约：后者覆盖，无自动 drop） */
    EXPECT_EQ(mmdb_rag_set_embedding(tc_, emb_a), MMDB_OK);
    EXPECT_EQ(mmdb_rag_set_embedding(tc_, emb_b), MMDB_OK);

    /* retrieve 仍可工作（行为正确性的间接验证） */
    mmdb_rag_query_t q = {0};
    q.query_text = "deep learning";
    q.top_k = 3;
    mmdb_rag_result_t r = {0};
    EXPECT_EQ(mmdb_rag_retrieve(tc_, &q, &r), MMDB_OK);
    EXPECT_EQ(r.items.count, q.top_k);

    mmdb_rag_result_free(&r);
    /* 所有权归调用方：手动释放两次设置的 embedding */
    mmdb_embedding_drop(emb_a);
    mmdb_embedding_drop(emb_b);
}

/* 负向用例：set_embedding(coll, NULL) 应清空 collection-level embedding，
 * 此后 retrieve 走 HASH fallback（q.embedding == NULL && coll->embedding == NULL） */
TEST_F(RagEmbeddingTest, NullEmbeddingClearsCollectionLevel) {
    mmdb_embedding_t* emb = mmdb_embedding_create(MMDB_EMBED_HASH, kDim);
    ASSERT_NE(emb, nullptr);

    EXPECT_EQ(mmdb_rag_set_embedding(tc_, emb), MMDB_OK);
    /* 传 NULL 等价于清除（API 契约） */
    EXPECT_EQ(mmdb_rag_set_embedding(tc_, nullptr), MMDB_OK);

    /* 清除后 retrieve 仍可工作（走 HASH fallback） */
    mmdb_rag_query_t q = {0};
    q.query_text = "vector database";
    q.top_k = 3;
    mmdb_rag_result_t r = {0};
    EXPECT_EQ(mmdb_rag_retrieve(tc_, &q, &r), MMDB_OK);
    EXPECT_EQ(r.items.count, q.top_k);

    mmdb_rag_result_free(&r);
    mmdb_embedding_drop(emb);
}
