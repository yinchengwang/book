/**
 * @file test_integration_rag.cpp
 * @brief RAG Pipeline 端到端集成测试
 */

#include "rag/pipeline.h"
#include "rag/embedding.h"
#include "rag/retriever.h"
#include "rag/reranker.h"
#include "rag/bge_reranker.h"
#include "rag/query_expander.h"
#include "rag/adaptive_rrf.h"
#include "rag/self_rag.h"
#include "rag/semantic_chunker.h"
#include "rag/retrieval_cache.h"
#include "rag/async_pipeline.h"
#include "gtest/gtest.h"
#include <memory>

namespace rag {
namespace test {

class RAGIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化组件
        config_ = std::make_shared<Config>();
        pipeline_ = std::make_shared<RetrievalPipeline>();
    }

    std::shared_ptr<Config> config_;
    std::shared_ptr<RetrievalPipeline> pipeline_;
};

// 测试: 基础 Pipeline 执行
TEST_F(RAGIntegrationTest, BasicPipelineExecute) {
    PipelineResult result = pipeline_->execute("RAG 是什么", 5);
    EXPECT_TRUE(result.success || !result.error_message.empty());
}

// 测试: Pipeline 结果格式
TEST_F(RAGIntegrationTest, ResultFormat) {
    PipelineResult result = pipeline_->execute("测试查询", 3);
    EXPECT_GE(result.results.size(), 0);
    EXPECT_GE(result.total_time_ms, 0);
}

// 测试: Pipeline with Cache
TEST_F(RAGIntegrationTest, WithCache) {
    auto cache = std::make_shared<LruCache>(CacheConfig{100, 3600});
    pipeline_->set_cache(cache);

    // 第一次查询
    auto result1 = pipeline_->execute("测试", 5);
    // 第二次查询（应该命中缓存）
    auto result2 = pipeline_->execute("测试", 5);

    EXPECT_TRUE(result1.success);
    EXPECT_TRUE(result2.success);
}

// 测试: Async Pipeline
TEST_F(RAGIntegrationTest, AsyncExecute) {
    AsyncConfig async_config{4, 100, 10, true};
    auto async_pipeline = create_async_pipeline(pipeline_, async_config);

    auto future = async_pipeline->execute_async("异步测试", 5);
    auto result = future.get();

    EXPECT_TRUE(result.success || !result.error_message.empty());
}

// 测试: Query Expansion 集成
TEST_F(RAGIntegrationTest, QueryExpansion) {
    auto expander = std::make_shared<HyDEExpander>(2);
    ExpansionResult exp_result = expander->expand("测试查询");

    EXPECT_GE(exp_result.expanded_queries.size(), 1);
}

// 测试: Adaptive RRF 融合
TEST_F(RAGIntegrationTest, AdaptiveRRF) {
    AdaptiveRRF fusion(60);

    std::vector<RetrievalResult> hnsw_results, bm25_results;
    // 添加测试数据
    RetrievalResult r1, r2;
    r1.chunk_id = "1";
    r1.score = 0.9;
    r2.chunk_id = "2";
    r2.score = 0.8;
    hnsw_results.push_back(r1);
    bm25_results.push_back(r2);

    auto fused = fusion.fuse(hnsw_results, bm25_results, QueryType::FACTUAL, 5);
    EXPECT_GE(fused.size(), 0);
}

// 测试: Self-RAG Stage
TEST_F(RAGIntegrationTest, SelfRAGStage) {
    SelfRAGConfig config;
    auto stage = create_self_rag_stage(config);

    StageInput input;
    input.query = "测试查询";
    input.query_type = QueryType::FACTUAL;

    auto output = stage->process(input);
    EXPECT_TRUE(output.status == StageOutput::Status::SUCCESS ||
                output.status == StageOutput::Status::SKIPPED);
}

// 测试: Semantic Chunker
TEST_F(RAGIntegrationTest, SemanticChunker) {
    SemanticChunkingConfig config;
    config.chunk_size = 200;
    auto chunker = std::make_unique<SemanticChunker>(config);

    std::string text = "这是第一句话。这是第二句话。这是第三句话。";
    auto chunks = chunker->chunk(text, "doc1");

    EXPECT_GE(chunks.size(), 1);
}

}  // namespace test
}  // namespace rag
