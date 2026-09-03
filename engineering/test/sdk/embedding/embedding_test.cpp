// SDK embedding 接口测试：hash 确定性 + L2 归一化 + 占位实现
#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
extern "C" {
#include "sdk/mmdb_embedding.h"
#include "sdk/mmdb_error.h"
}

// Hash 实现：同文本同向量，且 L2 归一化
TEST(Embedding, HashDeterministic) {
    auto* emb = mmdb_embedding_create(MMDB_EMBED_HASH, 128);
    ASSERT_NE(emb, nullptr);

    float v1[128], v2[128];
    ASSERT_EQ(mmdb_embed_text(emb, "hello world", 11, v1, 128), 0);
    ASSERT_EQ(mmdb_embed_text(emb, "hello world", 11, v2, 128), 0);

    for (int i = 0; i < 128; i++) {
        EXPECT_FLOAT_EQ(v1[i], v2[i]);
    }

    /* 验证 L2 归一化（norm ≈ 1.0） */
    float norm = 0;
    for (int i = 0; i < 128; i++) norm += v1[i] * v1[i];
    norm = std::sqrt(norm);
    EXPECT_NEAR(norm, 1.0f, 1e-4);

    mmdb_embedding_drop(emb);
}

// 不同文本应产生明显不同的向量
TEST(Embedding, HashDifferentTextDifferentVector) {
    auto* emb = mmdb_embedding_create(MMDB_EMBED_HASH, 128);
    float v1[128], v2[128];
    mmdb_embed_text(emb, "hello world", 11, v1, 128);
    mmdb_embed_text(emb, "goodbye world", 13, v2, 128);

    /* 计算余弦相似度：不同文本应 < 1.0 */
    float dot = 0;
    for (int i = 0; i < 128; i++) dot += v1[i] * v2[i];
    EXPECT_LT(dot, 0.99f) << "不同文本应产生不同向量";

    mmdb_embedding_drop(emb);
}

// AVERAGE_POOL 占位：返回 MMDB_ERR_NOT_IMPLEMENTED
TEST(Embedding, AveragePoolNotImplemented) {
    auto* emb = mmdb_embedding_create(MMDB_EMBED_AVERAGE_POOL, 128);
    ASSERT_NE(emb, nullptr);
    float v[128];
    EXPECT_EQ(mmdb_embed_text(emb, "x", 1, v, 128), MMDB_ERR_NOT_IMPLEMENTED);
    mmdb_embedding_drop(emb);
}

// OPENAI 占位：返回 MMDB_ERR_NOT_IMPLEMENTED
TEST(Embedding, OpenAINotImplemented) {
    auto* emb = mmdb_embedding_create(MMDB_EMBED_OPENAI, 128);
    ASSERT_NE(emb, nullptr);
    float v[128];
    EXPECT_EQ(mmdb_embed_text(emb, "x", 1, v, 128), MMDB_ERR_NOT_IMPLEMENTED);
    mmdb_embedding_drop(emb);
}

// out_dim 与 emb->dim 不匹配时返回负数
TEST(Embedding, DimensionMismatch) {
    auto* emb = mmdb_embedding_create(MMDB_EMBED_HASH, 64);
    float v[128];  /* dim 不匹配 */
    EXPECT_LT(mmdb_embed_text(emb, "x", 1, v, 128), 0);
    mmdb_embedding_drop(emb);
}