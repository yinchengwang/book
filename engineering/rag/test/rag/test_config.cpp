/**
 * @file test_config.cpp
 * @brief 配置管理测试
 */

#include <gtest/gtest.h>
#include "rag/config.h"

using namespace rag;

TEST(ConfigTest, DefaultConfig) {
    Config config = Config::default_config();

    EXPECT_EQ(config.version, "1.0.0");
    EXPECT_EQ(config.embedding.batch_size, 32);
    EXPECT_EQ(config.hnsw.dim, 768);
    EXPECT_EQ(config.retrieval.top_k, 5);
    EXPECT_EQ(config.chunking.chunk_size, 500);
}

TEST(ConfigTest, ConfigToString) {
    Config config = Config::default_config();
    std::string str = config.to_string();

    EXPECT_FALSE(str.empty());
    EXPECT_NE(str.find("RAG Config"), std::string::npos);
}

TEST(ConfigTest, ConfigValidation) {
    Config config = Config::default_config();

    // 默认配置应该有效
    EXPECT_TRUE(config.validate());

    // 无效维度应该失败
    Config invalid_config = Config::default_config();
    invalid_config.hnsw.dim = 0;
    EXPECT_FALSE(invalid_config.validate());
}

TEST(ConfigTest, DataSourceConfig) {
    DataSourceConfig source;
    source.path = "./docs";
    source.patterns = {"*.md", "*.txt"};
    source.recursive = true;

    EXPECT_EQ(source.path, "./docs");
    EXPECT_EQ(source.patterns.size(), 2);
    EXPECT_TRUE(source.recursive);
}

TEST(ConfigTest, EmbeddingConfig) {
    EmbeddingConfig embed;
    embed.model_path = "./models/bge";
    embed.batch_size = 64;
    embed.device = "cuda";

    EXPECT_EQ(embed.model_path, "./models/bge");
    EXPECT_EQ(embed.batch_size, 64);
    EXPECT_EQ(embed.device, "cuda");
}

TEST(ConfigTest, LLMConfig) {
    LLMConfig llm;
    llm.model_path = "./models/llama.gguf";
    llm.n_ctx = 4096;
    llm.max_tokens = 2048;
    llm.temperature = 0.8f;

    EXPECT_EQ(llm.n_ctx, 4096);
    EXPECT_EQ(llm.max_tokens, 2048);
    EXPECT_FLOAT_EQ(llm.temperature, 0.8f);
}

TEST(ConfigTest, HNSWConfig) {
    HNSWConfig hnsw;
    hnsw.dim = 1024;
    hnsw.m = 32;
    hnsw.ef_construction = 300;

    EXPECT_EQ(hnsw.dim, 1024);
    EXPECT_EQ(hnsw.m, 32);
    EXPECT_EQ(hnsw.ef_construction, 300);
}

TEST(ConfigTest, ChunkingConfig) {
    ChunkingConfig chunking;
    chunking.strategy = "fixed";
    chunking.chunk_size = 1000;
    chunking.chunk_overlap = 100;

    EXPECT_EQ(chunking.strategy, "fixed");
    EXPECT_EQ(chunking.chunk_size, 1000);
    EXPECT_EQ(chunking.chunk_overlap, 100);
}

TEST(ConfigTest, RetrievalConfig) {
    RetrievalConfig retrieval;
    retrieval.top_k = 10;
    retrieval.hybrid = true;
    retrieval.hybrid_weight = 0.6f;

    EXPECT_EQ(retrieval.top_k, 10);
    EXPECT_TRUE(retrieval.hybrid);
    EXPECT_FLOAT_EQ(retrieval.hybrid_weight, 0.6f);
}
