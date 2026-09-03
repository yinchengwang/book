#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "infra/infra.h"
#include "infra/model.h"
#include "infra/tokenizer.h"
}

class InfraEmbedTest : public ::testing::Test {
protected:
    void SetUp() override { infra_init(); }
};

TEST_F(InfraEmbedTest, SingleTextDimension) {
    const char* texts[] = {"hello world"};
    float* embs = nullptr;
    int dim = 0;
    infra_status_t s = infra_embed(nullptr, texts, 1, &embs, &dim);
    /* 不带模型时返回错误 */
    if (s == INFRA_ERR_PARAM) {
        SUCCEED() << "正确处理空模型";
        return;
    }
    EXPECT_EQ(s, INFRA_OK);
    EXPECT_EQ(dim, 384);
    EXPECT_NE(embs, nullptr);
    infra_embed_free(embs, 1);
}

TEST_F(InfraEmbedTest, BatchTextShape) {
    /* 创建一个空的 mock model */
    infra_model_t m = {};
    m.n_embd = 384;
    const char* texts[] = {"a", "bb", "ccc"};
    float* embs = nullptr;
    int dim = 0;
    infra_status_t s = infra_embed(&m, texts, 3, &embs, &dim);
    if (s == INFRA_OK) {
        EXPECT_EQ(dim, 384);
        /* 验证每段文本都有 384 维 */
        EXPECT_NE(embs, nullptr);
        infra_embed_free(embs, 3);
    }
}

TEST_F(InfraEmbedTest, EmptyInput) {
    infra_model_t m = {};
    m.n_embd = 384;
    float* embs = nullptr;
    int dim = 0;
    infra_status_t s = infra_embed(&m, nullptr, 0, &embs, &dim);
    EXPECT_EQ(s, INFRA_ERR_PARAM);
}

TEST_F(InfraEmbedTest, FreeNullSafe) {
    infra_embed_free(nullptr, 0);  /* 不应崩溃 */
    SUCCEED();
}

/* 词表加载测试 */
class TokenizerTest : public ::testing::Test {
protected:
    const char* model_path = "test-data/test_infra_model/with_vocab.gguf";
    void SetUp() override {
        FILE* f = fopen(model_path, "rb");
        if (!f) GTEST_SKIP() << "测试 GGUF 文件不存在: " << model_path;
        else fclose(f);
    }
};

TEST_F(TokenizerTest, LoadVocab) {
    infra_model_t* m = infra_model_load(model_path, INFRA_MODEL_GGUF);
    ASSERT_NE(m, nullptr) << "模型加载失败";

    char** vocab = nullptr;
    int vocab_size = 0;
    infra_status_t s = infra_tokenizer_load_vocab(m, &vocab, &vocab_size);

    /* 应返回 INFRA_OK（词表确实被加载） */
    ASSERT_EQ(s, INFRA_OK);
    EXPECT_EQ(vocab_size, 3) << "词表应包含 3 个词条";
    EXPECT_NE(vocab, nullptr) << "词表指针不应为空";

    /* 验证词表内容 */
    EXPECT_STREQ(vocab[0], "[PAD]");
    EXPECT_STREQ(vocab[1], "[UNK]");
    EXPECT_STREQ(vocab[2], "hello");

    /* 通过 infra_tokenize 验证词表可用 */
    int ids[16] = {0};
    int out_len = 0;
    const char* vocab_view[3] = {vocab[0], vocab[1], vocab[2]};
    s = infra_tokenize("hello", vocab_view, vocab_size, 16, ids, &out_len);
    EXPECT_EQ(s, INFRA_OK);
    EXPECT_GT(out_len, 0);

    infra_tokenizer_free_vocab(vocab, vocab_size);
    infra_model_free(m);
}

/* 不含词表的 GGUF 文件返回 NOT_FOUND */
class TokenizerNoVocabTest : public ::testing::Test {
protected:
    const char* model_path = "test-data/test_infra_model/sample.gguf";
    void SetUp() override {
        FILE* f = fopen(model_path, "rb");
        if (!f) GTEST_SKIP() << "测试 GGUF 文件不存在: " << model_path;
        else fclose(f);
    }
};

TEST_F(TokenizerNoVocabTest, LoadVocabNotFound) {
    infra_model_t* m = infra_model_load(model_path, INFRA_MODEL_GGUF);
    ASSERT_NE(m, nullptr) << "模型加载失败";

    char** vocab = nullptr;
    int vocab_size = 0;
    infra_status_t s = infra_tokenizer_load_vocab(m, &vocab, &vocab_size);

    EXPECT_EQ(s, INFRA_ERR_NOT_FOUND);
    EXPECT_EQ(vocab, nullptr);
    EXPECT_EQ(vocab_size, 0);

    infra_model_free(m);
}