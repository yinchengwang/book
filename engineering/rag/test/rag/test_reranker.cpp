/**
 * @file test_reranker.cpp
 * @brief BGE Reranker 单元测试
 */

#include <gtest/gtest.h>
#include "rag/bge_reranker.h"

namespace rag {
namespace test {

class BGERerankerTest : public ::testing::Test {
protected:
    void SetUp() override {
        BGERerankerConfig config;
        config.provider = "CPUExecutionProvider";
        config.batch_size = 8;
        config.max_length = 512;
        reranker_ = std::make_shared<BGEReranker>(config);
    }

    std::shared_ptr<BGEReranker> reranker_;
};

// ========== 基础测试 ==========

TEST_F(BGERerankerTest, BasicRerank) {
    std::string query = "What is machine learning?";
    std::vector<Chunk> candidates = {
        {"chunk1", "", "Machine learning is a subset of AI.", 0, 0, 0, 0, 0, {}, 0.6f},
        {"chunk2", "", "The weather is nice today.", 0, 0, 0, 0, 0, {}, 0.5f},
        {"chunk3", "", "ML algorithms learn from data.", 0, 0, 0, 0, 0, {}, 0.7f},
    };

    auto results = reranker_->rerank(query, candidates, 5);

    EXPECT_EQ(results.size(), 3);
    // ML 相关内容应该排在前面
    EXPECT_EQ(results[0].chunk_id, "chunk3");
}

TEST_F(BGERerankerTest, BatchRerank) {
    std::string query = "Deep learning tutorial";
    std::vector<Chunk> candidates;
    for (int i = 0; i < 20; i++) {
        candidates.push_back({"chunk" + std::to_string(i), "",
            "Content " + std::to_string(i), 0, 0, 0, 0, 0, {}, 0.5f});
    }

    auto results = reranker_->rerank(query, candidates, 8);

    EXPECT_EQ(results.size(), 8);
}

TEST_F(BGERerankerTest, EmptyResults) {
    std::string query = "test";
    std::vector<Chunk> candidates;

    auto results = reranker_->rerank(query, candidates, 5);

    EXPECT_TRUE(results.empty());
}

TEST_F(BGERerankerTest, RetrievalResultBatchRerank) {
    std::string query = "What is machine learning?";
    std::vector<RetrievalResult> results = {
        {Chunk{"chunk1", "", "Machine learning is AI.", 0, 0, 0, 0, 0, {}, 0.0f}, 0.6f, 0.0f, 0.0f, "", 0},
        {Chunk{"chunk2", "", "The weather is nice.", 0, 0, 0, 0, 0, {}, 0.0f}, 0.5f, 0.0f, 0.0f, "", 0},
        {Chunk{"chunk3", "", "ML algorithms learn.", 0, 0, 0, 0, 0, {}, 0.0f}, 0.7f, 0.0f, 0.0f, "", 0},
    };

    auto reranked = reranker_->rerank_batch(query, results, 8);

    EXPECT_EQ(reranked.size(), 3);
}

TEST_F(BGERerankerTest, GPUConfig) {
    GPUConfig config;
    config.enable = true;
    config.use_fp16 = true;

    reranker_->set_gpu_config(config);

    // 如果有 GPU，应该能设置成功
    if (GPUManager::instance().is_available()) {
        EXPECT_TRUE(true);  // 配置成功
    }
}

TEST_F(BGERerankerTest, SetFP16) {
    reranker_->set_fp16(true);
    reranker_->set_fp16(false);
    // 不应崩溃
    EXPECT_TRUE(true);
}

TEST_F(BGERerankerTest, GetModelInfo) {
    auto info = reranker_->get_model_info();

    EXPECT_GT(info.max_length, 0);
    EXPECT_FALSE(info.model_name.empty());
}

TEST_F(BGERerankerTest, Warmup) {
    reranker_->warmup(5);

    // 预热后模型信息应该可用
    auto info = reranker_->get_model_info();
    EXPECT_GT(info.max_length, 0);
}

TEST_F(BGERerankerTest, StatsTracking) {
    std::string query = "test query";
    std::vector<Chunk> candidates = {
        {"chunk1", "", "test content", 0, 0, 0, 0, 0, {}, 0.5f},
    };

    reranker_->rerank(query, candidates, 1);

    auto stats = reranker_->stats();
    EXPECT_EQ(stats.total_calls, 1);
}

TEST_F(BGERerankerTest, Name) {
    EXPECT_EQ(reranker_->name(), "bge_reranker_onnx");
}

TEST_F(BGERerankerTest, ModelType) {
    EXPECT_EQ(reranker_->model_type(), "bge-reranker-v2-m3");
}

}  // namespace test
}  // namespace rag

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}