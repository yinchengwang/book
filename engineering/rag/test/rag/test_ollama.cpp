/**
 * @file test_ollama.cpp
 * @brief Ollama 服务测试
 */

#include <gtest/gtest.h>
#include "rag/ollama_embedding.h"
#include "rag/ollama_llm.h"
#include "rag/embedding.h"

using namespace rag;

// ========== OllamaEmbeddingService 测试 ==========

class OllamaEmbeddingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建服务，配置较小的缓存以便测试
        config_ = OllamaEmbeddingConfig{
            "http://localhost:11434",
            "nomic-embed-text",
            100,  // 小缓存
            5,    // 短超时
            1     // 少重试
        };
    }

    OllamaEmbeddingConfig config_;
};

TEST_F(OllamaEmbeddingTest, FallbackOnServiceUnavailable) {
    // 当 Ollama 服务不可用时，应该自动降级到 SimpleEmbeddingService
    OllamaEmbeddingService service(config_);

    // 服务不可用时会使用降级服务
    // 这是预期的行为（因为测试环境可能没有 Ollama）
    EXPECT_TRUE(service.is_ready() || service.using_fallback());

    // 如果使用降级服务，fallback() 应该返回有效指针
    if (service.using_fallback()) {
        EXPECT_NE(service.fallback(), nullptr);
    }
}

TEST_F(OllamaEmbeddingTest, FallbackEncodeProducesVector) {
    OllamaEmbeddingService service(config_);

    std::string text = "测试文本";
    auto embedding = service.encode(text);

    // 向量维度应该正确
    EXPECT_EQ(static_cast<int>(embedding.size()), service.dimension());

    // 向量应该有值（不全为零）
    float sum = 0.0f;
    for (float v : embedding) {
        sum += std::abs(v);
    }
    EXPECT_GT(sum, 0.0f);
}

TEST_F(OllamaEmbeddingTest, FallbackBatchEncode) {
    OllamaEmbeddingService service(config_);

    std::vector<std::string> texts = {
        "第一个测试文本",
        "第二个测试文本",
        "第三个测试文本"
    };

    auto embeddings = service.encode_batch(texts);

    EXPECT_EQ(embeddings.size(), texts.size());
    for (const auto& emb : embeddings) {
        EXPECT_EQ(static_cast<int>(emb.size()), service.dimension());
    }
}

TEST_F(OllamaEmbeddingTest, CacheFunctionality) {
    OllamaEmbeddingService service(config_);

    std::string text = "缓存测试文本";

    // 第一次编码
    auto emb1 = service.encode(text);

    // 检查缓存统计
    auto stats = service.get_cache_stats();
    EXPECT_EQ(stats.size, 1);
    EXPECT_EQ(stats.hits, 0);
    EXPECT_EQ(stats.misses, 1);

    // 第二次编码应该命中缓存
    auto emb2 = service.encode(text);

    stats = service.get_cache_stats();
    EXPECT_EQ(stats.size, 1);
    EXPECT_EQ(stats.hits, 1);
    EXPECT_EQ(stats.misses, 1);

    // 向量应该相同
    EXPECT_EQ(emb1, emb2);
}

TEST_F(OllamaEmbeddingTest, ClearCache) {
    OllamaEmbeddingService service(config_);

    std::vector<std::string> texts = {"文本1", "文本2", "文本3"};

    // 编码多个文本
    service.encode_batch(texts);

    // 确认缓存有内容
    EXPECT_GT(service.cache_size(), 0);

    // 清空缓存
    service.clear_cache();

    // 缓存应该为空
    EXPECT_EQ(service.cache_size(), 0);
}

TEST_F(OllamaEmbeddingTest, DifferentTextsDifferentVectors) {
    OllamaEmbeddingService service(config_);

    auto emb1 = service.encode("文本 A");
    auto emb2 = service.encode("文本 B");

    // 不同文本应该产生不同的向量
    // 由于是随机向量，概率上几乎不可能完全相同
    EXPECT_NE(emb1, emb2);
}

TEST_F(OllamaEmbeddingTest, EmptyTextHandling) {
    OllamaEmbeddingService service(config_);

    auto embedding = service.encode("");

    // 空文本应该返回零向量
    EXPECT_EQ(embedding.size(), static_cast<size_t>(service.dimension()));
    for (float v : embedding) {
        EXPECT_EQ(v, 0.0f);
    }
}

TEST_F(OllamaEmbeddingTest, ModelName) {
    OllamaEmbeddingService service(config_);

    EXPECT_EQ(service.model_name(), "nomic-embed-text");
}

// ========== OllamaLLMService 测试 ==========

class OllamaLLMTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = OllamaLLMConfig{
            "http://localhost:11434",
            "llama3.2",
            10,  // 短超时（用于测试）
            1
        };
    }

    OllamaLLMConfig config_;
};

TEST_F(OllamaLLMTest, ServiceAvailability) {
    OllamaLLMService service(config_);

    // 服务可能不可用，这是正常的
    EXPECT_TRUE(service.is_loaded() || !service.is_service_ready());
}

TEST_F(OllamaLLMTest, FallbackGenerate) {
    OllamaLLMService service(config_);

    // 即使服务不可用，也应该能检查状态
    // 注意：实际生成需要服务可用
    if (!service.is_loaded()) {
        GTEST_SKIP() << "Ollama service not available, skipping generate test";
    }

    GenerateOptions options;
    options.max_tokens = 50;
    options.temperature = 0.7f;

    auto result = service.generate("Hello, how are you?", options);

    EXPECT_FALSE(result.text.empty());
    EXPECT_GT(result.tokens_generated, 0);
}

TEST_F(OllamaLLMTest, ContextWindow) {
    OllamaLLMService service(config_);

    // 上下文窗口应该有合理的值
    int ctx = service.context_window();
    EXPECT_GE(ctx, 1024);  // 至少 1K 上下文
}

TEST_F(OllamaLLMTest, ModelPath) {
    OllamaLLMService service(config_);

    EXPECT_EQ(service.model_path(), "llama3.2");
}

TEST_F(OllamaLLMTest, StatsTracking) {
    OllamaLLMService service(config_);

    auto stats = service.get_stats();

    // 初始统计应该是零
    EXPECT_EQ(stats.total_generations, 0);
    EXPECT_EQ(stats.total_tokens, 0);
    EXPECT_EQ(stats.avg_duration_ms, 0.0);
}

TEST_F(OllamaLLMTest, ResetStats) {
    OllamaLLMService service(config_);

    // 重置统计
    service.reset_stats();

    auto stats = service.get_stats();
    EXPECT_EQ(stats.total_generations, 0);
}

// ========== 集成测试（需要 Ollama） ==========

#ifdef RAG_TEST_INTEGRATION

TEST(OllamaIntegrationTest, RealEmbedding) {
    OllamaEmbeddingService service(
        "http://localhost:11434",
        "nomic-embed-text",
        1000);

    if (!service.is_ready()) {
        GTEST_SKIP() << "Ollama service not running";
    }

    auto emb1 = service.encode("What is RAG?");
    auto emb2 = service.encode("What is retrieval augmented generation?");

    // 相似文本应该有高相似度
    float sim = cosine_similarity(emb1, emb2);
    EXPECT_GT(sim, 0.5f);  // 至少 0.5 相似度
}

TEST(OllamaIntegrationTest, RealGeneration) {
    OllamaLLMService service(
        "http://localhost:11434",
        "llama3.2");

    if (!service.is_loaded()) {
        GTEST_SKIP() << "Ollama service not running";
    }

    GenerateOptions options;
    options.max_tokens = 100;
    options.temperature = 0.7f;

    auto result = service.generate(
        "Explain what RAG is in one sentence.",
        options);

    EXPECT_FALSE(result.text.empty());
    EXPECT_TRUE(result.text.find("RAG") != std::string::npos ||
                result.text.find("retrieval") != std::string::npos);
}

#endif  // RAG_TEST_INTEGRATION
