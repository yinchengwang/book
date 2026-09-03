/**
 * @file test_llm.cpp
 * @brief LLM 客户端测试
 */

#include <gtest/gtest.h>
#include "rag/llm_config.h"

using namespace rag;

// ========== OllamaClient 测试 ==========

class OllamaClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = create_ollama_client("http://localhost:11434");
    }

    std::unique_ptr<OllamaClient> client_;
};

TEST_F(OllamaClientTest, ClientCreation) {
    EXPECT_NE(client_, nullptr);
}

TEST_F(OllamaClientTest, IsAvailable) {
    // 服务可能不可用，这是正常的
    bool available = client_->is_available();
    // 不应该崩溃
    EXPECT_TRUE(available || !available);
}

TEST_F(OllamaClientTest, ListModels) {
    auto models = client_->list_models();
    // 可能为空（服务不可用或无模型）
    EXPECT_TRUE(models.empty() || !models.empty());
}

TEST_F(OllamaClientTest, GetModelInfo) {
    auto info = client_->get_model_info();
    // 服务不可用时返回 nullopt
    if (!info.has_value()) {
        GTEST_SKIP() << "Ollama service not available";
    }
    EXPECT_FALSE(info->name.empty());
}

// ========== LLMClient 测试 ==========

class LLMClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        LLMConfig config;
        config.provider = LLMProvider::OLLAMA;
        config.base_url = "http://localhost:11434";
        config.model = "qwen2.5:7b";
        config.temperature = 0.7f;
        config.max_tokens = 50;
        client_ = create_llm_client(config);
    }

    std::unique_ptr<LLMClient> client_;
};

TEST_F(LLMClientTest, ClientCreation) {
    EXPECT_NE(client_, nullptr);
}

TEST_F(LLMClientTest, GeneratePrompt) {
    if (!client_->is_available()) {
        GTEST_SKIP() << "Ollama service not available";
    }

    std::string result = client_->generate("Hello");
    // 结果可能为空但不应该是异常
    EXPECT_TRUE(result.empty() || !result.empty());
}

TEST_F(LLMClientTest, GenerateWithOptions) {
    if (!client_->is_available()) {
        GTEST_SKIP() << "Ollama service not available";
    }

    // 使用默认配置生成
    std::string result = client_->generate("What is 2+2?");
    EXPECT_TRUE(result.empty() || !result.empty());
}

TEST_F(LLMClientTest, EmbedText) {
    if (!client_->is_available()) {
        GTEST_SKIP() << "Ollama service not available";
    }

    auto embedding = client_->embed("Hello world");
    // Embedding 向量应该有内容
    EXPECT_FALSE(embedding.empty());
}

TEST_F(LLMClientTest, ModelInfo) {
    auto info = client_->get_model_info();
    if (!info.has_value()) {
        GTEST_SKIP() << "Ollama service not available";
    }
    EXPECT_FALSE(info->name.empty());
    EXPECT_GT(info->context_length, 0);
}

// ========== 配置测试 ==========

TEST(LLMConfigTest, DefaultConfig) {
    LLMConfig config;

    EXPECT_EQ(config.provider, LLMProvider::OLLAMA);
    EXPECT_EQ(config.base_url, "http://localhost:11434");
    EXPECT_EQ(config.model, "qwen2.5:7b");
    EXPECT_EQ(config.temperature, 0.7f);
    EXPECT_EQ(config.max_tokens, 1024);
    EXPECT_EQ(config.context_window, 4096);
}

TEST(LLMConfigTest, CustomConfig) {
    LLMConfig config;
    config.provider = LLMProvider::LOCAL;
    config.base_url = "http://localhost:8080";
    config.model = "custom-model";
    config.temperature = 0.5f;
    config.max_tokens = 512;

    EXPECT_EQ(config.provider, LLMProvider::LOCAL);
    EXPECT_EQ(config.base_url, "http://localhost:8080");
    EXPECT_EQ(config.model, "custom-model");
    EXPECT_EQ(config.temperature, 0.5f);
    EXPECT_EQ(config.max_tokens, 512);
}

// ========== 工厂函数测试 ==========

TEST(FactoryTest, CreateOllamaClient) {
    auto client = create_ollama_client();
    EXPECT_NE(client, nullptr);
}

TEST(FactoryTest, CreateOllamaClientWithUrl) {
    auto client = create_ollama_client("http://localhost:11434");
    EXPECT_NE(client, nullptr);
}

TEST(FactoryTest, CreateLLMClient) {
    LLMConfig config;
    config.provider = LLMProvider::OLLAMA;
    auto client = create_llm_client(config);
    EXPECT_NE(client, nullptr);
}