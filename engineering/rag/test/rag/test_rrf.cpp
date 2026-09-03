/**
 * @file test_rrf.cpp
 * @brief RRF 融合模块单元测试
 */

#include <gtest/gtest.h>
#include "rag/adaptive_rrf.h"

namespace rag {
namespace test {

// ========== 测试辅助函数 ==========

static Chunk make_chunk(const std::string& id, const std::string& content) {
    Chunk chunk;
    chunk.id = id;
    chunk.content = content;
    return chunk;
}

static RetrievalResult make_result(const std::string& id, float score) {
    RetrievalResult result;
    result.chunk = make_chunk(id, "Content for " + id);
    result.score = score;
    result.source = "test";
    result.rank = 0;
    return result;
}

// ========== AdaptiveRRF 事实型查询测试 ==========

class AdaptiveRRFFactual : public ::testing::Test {
protected:
    void SetUp() override {
        fusion_ = std::make_unique<AdaptiveRRF>(60);
    }

    std::unique_ptr<AdaptiveRRF> fusion_;
};

TEST_F(AdaptiveRRFFactual, GetWeightsReturnsCorrectValues) {
    auto weights = fusion_->get_weights(QueryType::FACTUAL);

    EXPECT_FLOAT_EQ(weights.hnsw_weight, 0.4f);
    EXPECT_FLOAT_EQ(weights.bm25_weight, 0.6f);
    EXPECT_FLOAT_EQ(weights.graph_weight, 0.0f);
}

TEST_F(AdaptiveRRFFactual, FuseWithTwoSources) {
    std::vector<RetrievalResult> hnsw = {
        make_result("h1", 0.9f),
        make_result("h2", 0.8f),
        make_result("h3", 0.7f),
    };

    std::vector<RetrievalResult> bm25 = {
        make_result("b1", 0.85f),
        make_result("b2", 0.75f),
        make_result("h1", 0.95f),  // 重叠
    };

    auto results = fusion_->fuse(hnsw, bm25, QueryType::FACTUAL, 5);

    EXPECT_EQ(results.size(), 5);
    // h1 出现在两个源中，应该排名靠前
    EXPECT_EQ(results[0].chunk.id, "h1");
}

TEST_F(AdaptiveRRFFactual, FuseWithEmptySource) {
    std::vector<RetrievalResult> hnsw = {
        make_result("h1", 0.9f),
    };

    std::vector<RetrievalResult> bm25;  // 空

    auto results = fusion_->fuse(hnsw, bm25, QueryType::FACTUAL, 5);

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].chunk.id, "h1");
}

TEST_F(AdaptiveRRFFactual, FuseWithTopK) {
    std::vector<RetrievalResult> hnsw = {
        make_result("h1", 0.9f),
        make_result("h2", 0.8f),
        make_result("h3", 0.7f),
        make_result("h4", 0.6f),
        make_result("h5", 0.5f),
    };

    std::vector<RetrievalResult> bm25 = {
        make_result("b1", 0.85f),
        make_result("b2", 0.75f),
    };

    auto results = fusion_->fuse(hnsw, bm25, QueryType::FACTUAL, 3);

    EXPECT_EQ(results.size(), 3);
}

TEST_F(AdaptiveRRFFactual, RRFScoresAreNormalized) {
    // 验证 RRF 得分计算
    float score1 = fusion_->rrf_score(0, 0.4f);  // rank=0, weight=0.4
    float score2 = fusion_->rrf_score(1, 0.4f);  // rank=1, weight=0.4

    // score1 > score2 (排名越高得分越高)
    EXPECT_GT(score1, score2);

    // 验证 RRF 公式: weight * (1 / (k + rank))
    float expected1 = 0.4f * (1.0f / (60.0f + 0.0f));
    float expected2 = 0.4f * (1.0f / (60.0f + 1.0f));
    EXPECT_FLOAT_EQ(score1, expected1);
    EXPECT_FLOAT_EQ(score2, expected2);
}

// ========== AdaptiveRRF 分析型查询测试 ==========

class AdaptiveRRFAnalytical : public ::testing::Test {
protected:
    void SetUp() override {
        fusion_ = std::make_unique<AdaptiveRRF>(60);
    }

    std::unique_ptr<AdaptiveRRF> fusion_;
};

TEST_F(AdaptiveRRFAnalytical, GetWeightsReturnsCorrectValues) {
    auto weights = fusion_->get_weights(QueryType::ANALYTICAL);

    EXPECT_FLOAT_EQ(weights.hnsw_weight, 0.7f);
    EXPECT_FLOAT_EQ(weights.bm25_weight, 0.3f);
    EXPECT_FLOAT_EQ(weights.graph_weight, 0.0f);
}

TEST_F(AdaptiveRRFAnalytical, FuseWithAnalyticalQuery) {
    // HNSW 权重更高 (0.7 vs 0.3)
    std::vector<RetrievalResult> hnsw = {
        make_result("h1", 0.9f),
        make_result("h2", 0.8f),
    };

    std::vector<RetrievalResult> bm25 = {
        make_result("b1", 0.95f),
        make_result("b2", 0.85f),
    };

    auto results = fusion_->fuse(hnsw, bm25, QueryType::ANALYTICAL, 4);

    EXPECT_EQ(results.size(), 4);
    // h1 应该有更高融合得分因为 HNSW 权重更高
}

// ========== AdaptiveRRF 多跳查询测试 ==========

class AdaptiveRRFMultiHop : public ::testing::Test {
protected:
    void SetUp() override {
        fusion_ = std::make_unique<AdaptiveRRF>(60);
    }

    std::unique_ptr<AdaptiveRRF> fusion_;
};

TEST_F(AdaptiveRRFMultiHop, GetWeightsReturnsCorrectValues) {
    auto weights = fusion_->get_weights(QueryType::MULTI_HOP);

    EXPECT_FLOAT_EQ(weights.hnsw_weight, 0.3f);
    EXPECT_FLOAT_EQ(weights.bm25_weight, 0.2f);
    EXPECT_FLOAT_EQ(weights.graph_weight, 0.5f);
}

TEST_F(AdaptiveRRFMultiHop, FuseWithGraph) {
    std::vector<RetrievalResult> hnsw = {
        make_result("h1", 0.9f),
    };

    std::vector<RetrievalResult> bm25 = {
        make_result("b1", 0.85f),
    };

    std::vector<RetrievalResult> graph = {
        make_result("g1", 0.95f),
        make_result("g2", 0.90f),
    };

    auto results = fusion_->fuse_with_graph(hnsw, bm25, graph, QueryType::MULTI_HOP, 4);

    EXPECT_EQ(results.size(), 4);
    // graph 结果应该排名靠前因为权重最高 (0.5)
    EXPECT_EQ(results[0].chunk.id, "g1");
    EXPECT_EQ(results[1].chunk.id, "g2");
}

TEST_F(AdaptiveRRFMultiHop, GraphOnlyFusion) {
    std::vector<RetrievalResult> hnsw;
    std::vector<RetrievalResult> bm25;
    std::vector<RetrievalResult> graph = {
        make_result("g1", 0.9f),
        make_result("g2", 0.8f),
    };

    auto results = fusion_->fuse_with_graph(hnsw, bm25, graph, QueryType::MULTI_HOP, 2);

    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].chunk.id, "g1");
    EXPECT_EQ(results[1].chunk.id, "g2");
}

// ========== 置信度加权融合测试 ==========

class ConfidenceWeightedFusionTest : public ::testing::Test {
protected:
    void SetUp() override {
        fusion_ = std::make_unique<ConfidenceWeightedFusion>(60);
    }

    std::unique_ptr<ConfidenceWeightedFusion> fusion_;
};

TEST_F(ConfidenceWeightedFusionTest, FuseWithConfidence) {
    std::vector<RetrievalResult> hnsw = {
        make_result("h1", 0.9f),
        make_result("h2", 0.8f),
    };

    std::vector<RetrievalResult> bm25 = {
        make_result("b1", 0.85f),
        make_result("b2", 0.75f),
    };

    // HNSW 置信度高，BM25 置信度低
    auto results = fusion_->fuse(hnsw, bm25, 0.9f, 0.3f, 4);

    EXPECT_EQ(results.size(), 4);
    // h1 出现在 HNSW 中且 HNSW 置信度高
}

TEST_F(ConfidenceWeightedFusionTest, FuseWithZeroConfidence) {
    std::vector<RetrievalResult> hnsw = {
        make_result("h1", 0.9f),
    };

    std::vector<RetrievalResult> bm25 = {
        make_result("b1", 0.85f),
    };

    // 一个源置信度为 0
    auto results = fusion_->fuse(hnsw, bm25, 0.0f, 1.0f, 4);

    EXPECT_EQ(results.size(), 2);
    // 只有 bm25 的结果应该有融合得分
    EXPECT_EQ(results[0].chunk.id, "b1");
}

TEST_F(ConfidenceWeightedFusionTest, EmptyResults) {
    std::vector<RetrievalResult> hnsw;
    std::vector<RetrievalResult> bm25;

    auto results = fusion_->fuse(hnsw, bm25, 0.5f, 0.5f, 5);

    EXPECT_TRUE(results.empty());
}

// ========== RRF 归一化测试 ==========

class RRFNormalization : public ::testing::Test {
protected:
    void SetUp() override {
        fusion_ = std::make_unique<AdaptiveRRF>(60);
    }

    std::unique_ptr<AdaptiveRRF> fusion_;
};

TEST_F(RRFNormalization, RRFKSetterGetter) {
    fusion_->set_rrf_k(100);
    EXPECT_EQ(fusion_->rrf_k(), 100);

    fusion_->set_rrf_k(60);
    EXPECT_EQ(fusion_->rrf_k(), 60);
}

TEST_F(RRFNormalization, DifferentKValues) {
    auto fusion60 = std::make_unique<AdaptiveRRF>(60);
    auto fusion100 = std::make_unique<AdaptiveRRF>(100);

    float score60 = fusion60->rrf_score(0, 0.5f);
    float score100 = fusion100->rrf_score(0, 0.5f);

    // 更大的 k 值意味着更小的得分
    EXPECT_LT(score100, score60);

    // 验证公式
    EXPECT_FLOAT_EQ(score60, 0.5f * (1.0f / 60.0f));
    EXPECT_FLOAT_EQ(score100, 0.5f * (1.0f / 100.0f));
}

TEST_F(RRFNormalization, AllQueryTypesHaveWeights) {
    std::vector<QueryType> types = {
        QueryType::FACTUAL,
        QueryType::ANALYTICAL,
        QueryType::MULTI_HOP,
        QueryType::COMPARATIVE,
        QueryType::SUMMARY,
        QueryType::CHAT
    };

    for (auto type : types) {
        auto weights = fusion_->get_weights(type);
        // 权重应该为正数
        EXPECT_GE(weights.hnsw_weight, 0.0f);
        EXPECT_GE(weights.bm25_weight, 0.0f);
        EXPECT_GE(weights.graph_weight, 0.0f);
        // 总权重应该大于 0
        EXPECT_GT(weights.hnsw_weight + weights.bm25_weight + weights.graph_weight, 0.0f);
    }
}

// ========== 三路融合测试 ==========

class FuseWithGraph : public ::testing::Test {
protected:
    void SetUp() override {
        fusion_ = std::make_unique<AdaptiveRRF>(60);
    }

    std::unique_ptr<AdaptiveRRF> fusion_;
};

TEST_F(FuseWithGraph, ThreeWayFusion) {
    std::vector<RetrievalResult> hnsw = {
        make_result("h1", 0.9f),
        make_result("h2", 0.8f),
    };

    std::vector<RetrievalResult> bm25 = {
        make_result("b1", 0.85f),
        make_result("h1", 0.88f),  // 重叠
    };

    std::vector<RetrievalResult> graph = {
        make_result("g1", 0.95f),
        make_result("g2", 0.85f),
    };

    auto results = fusion_->fuse_with_graph(hnsw, bm25, graph, QueryType::COMPARATIVE, 5);

    EXPECT_EQ(results.size(), 5);
    // 验证排名
    EXPECT_EQ(results[0].rank, 1);
    EXPECT_EQ(results[1].rank, 2);
}

TEST_F(FuseWithGraph, ComparativeQueryWeights) {
    auto weights = fusion_->get_weights(QueryType::COMPARATIVE);

    EXPECT_FLOAT_EQ(weights.hnsw_weight, 0.5f);
    EXPECT_FLOAT_EQ(weights.bm25_weight, 0.3f);
    EXPECT_FLOAT_EQ(weights.graph_weight, 0.2f);
}

TEST_F(FuseWithGraph, DuplicateChunksGetCombinedScore) {
    std::string chunk_id = "common";
    Chunk common_chunk = make_chunk(chunk_id, "Common content");

    std::vector<RetrievalResult> hnsw = {
        {common_chunk, 0.9f, 0.0f, 0.0f, "hnsw", 1}
    };

    std::vector<RetrievalResult> bm25 = {
        {common_chunk, 0.85f, 0.0f, 0.0f, "bm25", 1}
    };

    std::vector<RetrievalResult> graph = {
        {common_chunk, 0.80f, 0.0f, 0.0f, "graph", 1}
    };

    auto results = fusion_->fuse_with_graph(hnsw, bm25, graph, QueryType::COMPARATIVE, 1);

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].chunk.id, chunk_id);
    // 得分应该是三个源的 RRF 得分之和
    // Comparative: hnsw=0.5, bm25=0.3, graph=0.2
    float expected_score = 0.5f * (1.0f / 61.0f) +  // rank=0
                           0.3f * (1.0f / 61.0f) +
                           0.2f * (1.0f / 61.0f);
    EXPECT_FLOAT_EQ(results[0].score, expected_score);
}

// ========== 工厂函数测试 ==========

TEST(AdaptiveRRFFactory, CreateAdaptiveRRF) {
    auto fusion = create_adaptive_rrf(60);
    EXPECT_NE(fusion, nullptr);

    auto weights = fusion->get_weights(QueryType::FACTUAL);
    EXPECT_FLOAT_EQ(weights.hnsw_weight, 0.4f);
    EXPECT_FLOAT_EQ(weights.bm25_weight, 0.6f);
}

TEST(AdaptiveRRFFactory, CreateConfidenceFusion) {
    auto fusion = create_confidence_fusion(100);
    EXPECT_NE(fusion, nullptr);

    fusion->set_rrf_k(100);
    EXPECT_EQ(fusion->rrf_k(), 100);
}

}  // namespace test
}  // namespace rag